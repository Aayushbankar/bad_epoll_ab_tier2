// test_hyp001_timerfd.c — HYP-001: Timerfd Interrupt Widening Characterization under QEMU TCG
// Evaluates whether timerfd-based interrupt widening behaves measurably differently
// under QEMU TCG to explain race non-reproduction.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <sys/timerfd.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/reboot.h>
#include <stdatomic.h>

#define N_SAMPLES 1000
#define NUM_INTERVALS 8

static const long INTERVALS_US[NUM_INTERVALS] = {
    1,       // 1 us (NAT-001 storm target)
    10,      // 10 us
    50,      // 50 us
    100,     // 100 us
    500,     // 500 us
    1000,    // 1 ms
    5000,    // 5 ms
    10000    // 10 ms
};

typedef struct {
    long target_us;
    int samples;
    double min_us;
    double max_us;
    double mean_us;
    double stddev_us;
    double median_us;
    double p90_us;
    double p99_us;
    uint64_t total_expirations;
    double expansion_ratio;
} LatencyStats;

static void print_str(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

static int compare_doubles(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

static inline uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

// Read /proc/interrupts looking for timer interrupts
static void dump_timer_interrupts(const char *tag) {
    char buf[2048] = {0};
    int fd = open("/proc/interrupts", O_RDONLY);
    if (fd < 0) return;
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return;
    buf[n] = '\0';

    char log_buf[256];
    snprintf(log_buf, sizeof(log_buf), "[*] Interrupt snapshot (%s):\n", tag);
    print_str(log_buf);

    char *line = strtok(buf, "\n");
    while (line) {
        if (strstr(line, "arch_timer") || strstr(line, "timer") || strstr(line, "IPI")) {
            print_str("    ");
            print_str(line);
            print_str("\n");
        }
        line = strtok(NULL, "\n");
    }
}

// Characterize a single timerfd interval
static LatencyStats test_interval(long interval_us, int samples) {
    LatencyStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.target_us = interval_us;
    stats.samples = samples;

    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd < 0) {
        print_str("[!] Error: timerfd_create failed\n");
        return stats;
    }

    struct itimerspec its;
    its.it_interval.tv_sec = interval_us / 1000000;
    its.it_interval.tv_nsec = (interval_us % 1000000) * 1000;
    its.it_value = its.it_interval;

    if (timerfd_settime(tfd, 0, &its, NULL) < 0) {
        print_str("[!] Error: timerfd_settime failed\n");
        close(tfd);
        return stats;
    }

    // Allocate array for sample collection
    double *deltas = malloc(sizeof(double) * samples);
    if (!deltas) {
        close(tfd);
        return stats;
    }

    uint64_t total_exp = 0;
    uint64_t t_prev = get_time_ns();

    // Warmup: discard first 5 iterations
    for (int i = 0; i < 5; i++) {
        uint64_t exp;
        read(tfd, &exp, sizeof(exp));
    }
    t_prev = get_time_ns();

    for (int i = 0; i < samples; i++) {
        uint64_t exp = 0;
        ssize_t s = read(tfd, &exp, sizeof(exp));
        uint64_t t_curr = get_time_ns();

        if (s == sizeof(exp)) {
            total_exp += exp;
            double dt_us = (double)(t_curr - t_prev) / 1000.0;
            deltas[i] = dt_us;
        } else {
            deltas[i] = 0.0;
        }
        t_prev = t_curr;
    }
    close(tfd);

    stats.total_expirations = total_exp;

    // Calculate metrics
    double sum = 0;
    double min_val = deltas[0];
    double max_val = deltas[0];

    for (int i = 0; i < samples; i++) {
        sum += deltas[i];
        if (deltas[i] < min_val) min_val = deltas[i];
        if (deltas[i] > max_val) max_val = deltas[i];
    }
    double mean = sum / samples;

    double sum_sq_diff = 0;
    for (int i = 0; i < samples; i++) {
        double diff = deltas[i] - mean;
        sum_sq_diff += diff * diff;
    }
    double stddev = sqrt(sum_sq_diff / samples);

    qsort(deltas, samples, sizeof(double), compare_doubles);
    double median = deltas[samples / 2];
    double p90 = deltas[(int)(samples * 0.90)];
    double p99 = deltas[(int)(samples * 0.99)];

    stats.min_us = min_val;
    stats.max_us = max_val;
    stats.mean_us = mean;
    stats.stddev_us = stddev;
    stats.median_us = median;
    stats.p90_us = p90;
    stats.p99_us = p99;
    stats.expansion_ratio = (interval_us > 0) ? (mean / (double)interval_us) : 0.0;

    free(deltas);
    return stats;
}

// ─────────────────────────────────────────────────────────────
// Part B: NAT-001 Timer Storm Replication
// Thread D on CPU 1 creates 100 timerfds at 1us interval
// Thread A on CPU 0 measures simulated critical sections
// ─────────────────────────────────────────────────────────────

#define STORM_TFDS 100
#define CRITICAL_SECTION_TRIALS 10000

static atomic_int storm_running = 0;
static atomic_int storm_ready = 0;
static atomic_uint_fast64_t storm_total_wakeups = 0;
static atomic_uint_fast64_t storm_total_expirations = 0;

static void *timer_storm_worker(void *arg) {
    (void)arg;
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    int tfds[STORM_TFDS];
    struct itimerspec its;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 1000; // 1 us
    its.it_value = its.it_interval;

    for (int i = 0; i < STORM_TFDS; i++) {
        tfds[i] = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (tfds[i] >= 0) {
            timerfd_settime(tfds[i], 0, &its, NULL);
        }
    }

    atomic_store(&storm_ready, 1);
    while (!atomic_load(&storm_running)) {
        sched_yield();
    }

    uint64_t wakeups = 0;
    uint64_t expirations = 0;

    while (atomic_load(&storm_running)) {
        for (int i = 0; i < STORM_TFDS; i++) {
            if (tfds[i] >= 0) {
                uint64_t exp = 0;
                if (read(tfds[i], &exp, sizeof(exp)) > 0) {
                    wakeups++;
                    expirations += exp;
                }
            }
        }
        sched_yield();
    }

    atomic_store(&storm_total_wakeups, wakeups);
    atomic_store(&storm_total_expirations, expirations);

    for (int i = 0; i < STORM_TFDS; i++) {
        if (tfds[i] >= 0) close(tfds[i]);
    }
    return NULL;
}

// Simulate a critical section of roughly 500 cycles on ARM64
// and measure elapsed duration in nanoseconds.
static inline uint64_t run_critical_section(void) {
    uint64_t t0 = get_time_ns();
    // Execute ~500 arithmetic instructions
    __asm__ volatile(
        "mov x0, #0\n\t"
        "mov x1, #0\n\t"
        ".rept 250\n\t"
        "add x0, x0, #1\n\t"
        "add x1, x1, x0\n\t"
        ".endr\n\t"
        :
        :
        : "x0", "x1", "cc"
    );
    uint64_t t1 = get_time_ns();
    return t1 - t0;
}

typedef struct {
    double min_ns;
    double max_ns;
    double mean_ns;
    double stddev_ns;
    double p50_ns;
    double p99_ns;
    int widened_count; // critical sections exceeding 2x mean
} CriticalStats;

static CriticalStats test_critical_sections(bool with_storm) {
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(0, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    double *samples = malloc(sizeof(double) * CRITICAL_SECTION_TRIALS);

    pthread_t storm_th;
    if (with_storm) {
        atomic_store(&storm_ready, 0);
        atomic_store(&storm_running, 0);
        pthread_create(&storm_th, NULL, timer_storm_worker, NULL);
        while (!atomic_load(&storm_ready)) usleep(100);
        atomic_store(&storm_running, 1);
    }

    // Warmup
    for (int i = 0; i < 100; i++) {
        run_critical_section();
    }

    for (int i = 0; i < CRITICAL_SECTION_TRIALS; i++) {
        uint64_t dt = run_critical_section();
        samples[i] = (double)dt;
    }

    if (with_storm) {
        atomic_store(&storm_running, 0);
        pthread_join(storm_th, NULL);
    }

    double sum = 0, min_v = samples[0], max_v = samples[0];
    for (int i = 0; i < CRITICAL_SECTION_TRIALS; i++) {
        sum += samples[i];
        if (samples[i] < min_v) min_v = samples[i];
        if (samples[i] > max_v) max_v = samples[i];
    }
    double mean = sum / CRITICAL_SECTION_TRIALS;

    double sum_sq = 0;
    for (int i = 0; i < CRITICAL_SECTION_TRIALS; i++) {
        double diff = samples[i] - mean;
        sum_sq += diff * diff;
    }
    double stddev = sqrt(sum_sq / CRITICAL_SECTION_TRIALS);

    qsort(samples, CRITICAL_SECTION_TRIALS, sizeof(double), compare_doubles);
    double p50 = samples[CRITICAL_SECTION_TRIALS / 2];
    double p99 = samples[(int)(CRITICAL_SECTION_TRIALS * 0.99)];

    int widened = 0;
    for (int i = 0; i < CRITICAL_SECTION_TRIALS; i++) {
        if (samples[i] > 2.0 * p50) {
            widened++;
        }
    }

    CriticalStats res;
    res.min_ns = min_v;
    res.max_ns = max_v;
    res.mean_ns = mean;
    res.stddev_ns = stddev;
    res.p50_ns = p50;
    res.p99_ns = p99;
    res.widened_count = widened;

    free(samples);
    return res;
}

int main(void) {
    char out[512];
    print_str("================================================================\n");
    print_str("HYP-001: Timerfd Interrupt Widening Characterization under QEMU TCG\n");
    print_str("================================================================\n\n");

    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/sys", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);

    dump_timer_interrupts("BASELINE");

    print_str("\n--- PART 1: Latency & Jitter vs Target Interval (N=1000 per tier) ---\n");
    print_str(
        "target_us | samples | min_us     | max_us     | mean_us    | stddev_us  | median_us  | p99_us     | coalesced_ticks | expansion_ratio\n"
        "----------+---------+------------+------------+------------+------------+------------+------------+-----------------+----------------\n"
    );

    LatencyStats all_stats[NUM_INTERVALS];
    for (int i = 0; i < NUM_INTERVALS; i++) {
        all_stats[i] = test_interval(INTERVALS_US[i], N_SAMPLES);
        snprintf(out, sizeof(out),
            "%9ld | %7d | %10.2f | %10.2f | %10.2f | %10.2f | %10.2f | %10.2f | %15llu | %15.2fx\n",
            all_stats[i].target_us,
            all_stats[i].samples,
            all_stats[i].min_us,
            all_stats[i].max_us,
            all_stats[i].mean_us,
            all_stats[i].stddev_us,
            all_stats[i].median_us,
            all_stats[i].p99_us,
            (unsigned long long)all_stats[i].total_expirations,
            all_stats[i].expansion_ratio
        );
        print_str(out);
    }

    dump_timer_interrupts("AFTER_PART1");

    print_str("\n--- PART 2: NAT-001 Timer Storm Replication on Critical Section ---\n");
    print_str("[*] Simulating ~500-cycle critical window on CPU 0 (10,000 trials)...\n");

    print_str("[*] Running Baseline (No Storm on CPU 1)...\n");
    CriticalStats base_crit = test_critical_sections(false);
    snprintf(out, sizeof(out),
        "[BASELINE] Critical Window: min=%.1fns | max=%.1fns | mean=%.1fns | stddev=%.1fns | p50=%.1fns | p99=%.1fns | widened (>2x p50): %d/%d (%.2f%%)\n",
        base_crit.min_ns, base_crit.max_ns, base_crit.mean_ns, base_crit.stddev_ns,
        base_crit.p50_ns, base_crit.p99_ns, base_crit.widened_count, CRITICAL_SECTION_TRIALS,
        (double)base_crit.widened_count * 100.0 / CRITICAL_SECTION_TRIALS
    );
    print_str(out);

    print_str("[*] Running With Active NAT-001 Timer Storm (100 timerfds @ 1us on CPU 1)...\n");
    CriticalStats storm_crit = test_critical_sections(true);
    snprintf(out, sizeof(out),
        "[STORM]    Critical Window: min=%.1fns | max=%.1fns | mean=%.1fns | stddev=%.1fns | p50=%.1fns | p99=%.1fns | widened (>2x p50): %d/%d (%.2f%%)\n",
        storm_crit.min_ns, storm_crit.max_ns, storm_crit.mean_ns, storm_crit.stddev_ns,
        storm_crit.p50_ns, storm_crit.p99_ns, storm_crit.widened_count, CRITICAL_SECTION_TRIALS,
        (double)storm_crit.widened_count * 100.0 / CRITICAL_SECTION_TRIALS
    );
    print_str(out);

    uint64_t wakeups = atomic_load(&storm_total_wakeups);
    uint64_t total_exp = atomic_load(&storm_total_expirations);
    snprintf(out, sizeof(out),
        "[*] Storm Activity (CPU 1): wakeups=%llu, total_expirations=%llu, coalescing_ratio=%.2f ticks/wakeup\n",
        (unsigned long long)wakeups, (unsigned long long)total_exp,
        wakeups > 0 ? ((double)total_exp / (double)wakeups) : 0.0
    );
    print_str(out);

    dump_timer_interrupts("FINAL");

    print_str("\n================================================================\n");
    print_str("HYP-001 SUMMARY & CONCLUSION\n");
    print_str("================================================================\n");

    snprintf(out, sizeof(out),
        "Target 1us delivery under QEMU TCG: mean=%.2fus, min=%.2fus, stddev=%.2fus (Expansion: %.1fx)\n",
        all_stats[0].mean_us, all_stats[0].min_us, all_stats[0].stddev_us, all_stats[0].expansion_ratio
    );
    print_str(out);

    double storm_widening_delta = storm_crit.mean_ns - base_crit.mean_ns;
    snprintf(out, sizeof(out),
        "Critical section mean duration delta under storm: %.1fns (Baseline: %.1fns -> Storm: %.1fns)\n",
        storm_widening_delta, base_crit.mean_ns, storm_crit.mean_ns
    );
    print_str(out);

    if (all_stats[0].expansion_ratio > 10.0) {
        print_str("[+] HYP-001 CONFIRMED: QEMU TCG cannot deliver microsecond-scale timerfd interrupts.\n");
        print_str("    The 1us timer storm requested in NAT-001 is quantized/delayed by orders of magnitude,\n");
        print_str("    demonstrating that timerfd-based interrupt widening is ineffective under TCG.\n");
    } else {
        print_str("[-] HYP-001 REJECTED: QEMU TCG delivers timerfd interrupts within nominal bounds.\n");
    }
    print_str("================================================================\n");

    reboot(RB_POWER_OFF);
    return 0;
}
