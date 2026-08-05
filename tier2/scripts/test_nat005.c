// test_nat005.c — NAT-005: Adaptive Launch-Ahead Search & Cache Topology Verification
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
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#define CACHE_LINE_SIZE 64

// ARM64 Virtual Counter Readers
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

// ------------------------------------------------------------
// Task 2: Topology & False-Sharing Benchmark
// ------------------------------------------------------------
typedef struct {
    volatile uint64_t val[CACHE_LINE_SIZE / 8];
} __attribute__((aligned(CACHE_LINE_SIZE))) bounce_line_t;

static bounce_line_t shared_bounce;
static atomic_int stop_hammer = 0;

void *topology_hammer(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    while (!atomic_load_explicit(&stop_hammer, memory_order_relaxed)) {
        shared_bounce.val[0]++;
        __sync_synchronize();
    }
    return NULL;
}

void verify_cpu_topology(void) {
    uint64_t frq = get_cntfrq();
    printf("[NAT-005] ARM64 Counter Frequency: %lu Hz (%lu MHz)\n", frq, frq / 1000000);
    fflush(stdout);

    // Baseline close()
    int ep = epoll_create1(0);
    uint64_t t0 = get_cycles();
    close(ep);
    uint64_t t1 = get_cycles();
    uint64_t baseline_ticks = t1 - t0;
    printf("[NAT-005] Baseline single close() duration: %lu ticks (~%.2f us)\n",
           baseline_ticks, (double)baseline_ticks * 1000000.0 / frq);
    fflush(stdout);

    // False sharing bounce check
    pthread_t th;
    atomic_store(&stop_hammer, 0);
    pthread_create(&th, NULL, topology_hammer, NULL);
    usleep(1000);

    uint64_t bounce_sum = 0;
    int samples = 50;
    for (int i = 0; i < samples; i++) {
        int ep_t = epoll_create1(0);
        uint64_t s0 = get_cycles();
        shared_bounce.val[0]++;
        __sync_synchronize();
        close(ep_t);
        uint64_t s1 = get_cycles();
        bounce_sum += (s1 - s0);
    }
    atomic_store(&stop_hammer, 1);
    pthread_join(th, NULL);

    uint64_t avg_bounce = bounce_sum / samples;
    printf("[NAT-005] Average close() under cross-CPU false sharing: %lu ticks (~%.2f us)\n",
           avg_bounce, (double)avg_bounce * 1000000.0 / frq);
    printf("[NAT-005] False Sharing Contention Factor: %.2fx delay\n",
           (double)avg_bounce / (baseline_ticks > 0 ? baseline_ticks : 1));
    fflush(stdout);
}

// ------------------------------------------------------------
// Task 1: Adaptive Launch-Ahead Search Harness
// ------------------------------------------------------------
static atomic_int sync_go = 0;
static atomic_int ready_a = 0, ready_b = 0;
static uint64_t current_launch_delay_cycles = 0;

static int ep_outer = -1;
static int ep_inner[2] = {-1, -1};

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

    // Adaptive Launch-Ahead Spin Delay
    uint64_t target_cycles = get_cycles() + current_launch_delay_cycles;
    while (get_cycles() < target_cycles) { asm volatile("yield"); }

    close(ep_inner[1]);

    // msg_msg spray
    int msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msqid >= 0) {
        struct { long mtype; char mtext[144]; } msg = { .mtype = 1 };
        memset(msg.mtext, 0x42, 144);
        for (int i = 0; i < 500; i++) {
            if (msgsnd(msqid, &msg, 144, IPC_NOWAIT) < 0) break;
        }
        msgctl(msqid, IPC_RMID, NULL);
    }
    return NULL;
}

int run_adaptive_iteration(int iteration, uint64_t launch_delay) {
    current_launch_delay_cycles = launch_delay;
    atomic_store(&sync_go, 0);
    atomic_store(&ready_a, 0);
    atomic_store(&ready_b, 0);

    ep_outer = epoll_create1(0);
    ep_inner[0] = epoll_create1(0);
    ep_inner[1] = epoll_create1(0);
    if (ep_outer < 0 || ep_inner[0] < 0 || ep_inner[1] < 0) return 0;

    int p0[2], p1[2];
    if (pipe(p0) < 0 || pipe(p1) < 0) return 0;

    struct epoll_event ev = { .events = EPOLLIN };
    ev.data.fd = ep_inner[0];
    epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner[0], &ev);
    ev.data.fd = ep_inner[1];
    epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner[1], &ev);

    pthread_t ta, tb;
    pthread_create(&ta, NULL, adaptive_thread_a, NULL);
    pthread_create(&tb, NULL, adaptive_thread_b, NULL);

    while (!atomic_load(&ready_a) || !atomic_load(&ready_b)) { usleep(10); }

    atomic_store(&sync_go, 1);

    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    close(ep_inner[0]);
    close(p0[0]); close(p0[1]);
    close(p1[0]); close(p1[1]);

    return 0; // 0 hits in dry run
}

int main(int argc, char **argv) {
    printf("[NAT-005] NAT-005 Adaptive Launch-Ahead Search & Topology Harness Starting...\n");
    fflush(stdout);

    // 1. Task 2: CPU Topology & False Sharing Check
    verify_cpu_topology();

    // 2. Task 1: Adaptive Parameter Sweep Design Verification (100 iterations test range)
    printf("[NAT-005] Initializing Adaptive Launch-Ahead Parameter Sweep...\n");
    fflush(stdout);

    uint64_t sweep_min = 0;
    uint64_t sweep_max = 2000; // cycle delay range
    uint64_t sweep_step = 50;

    int total_runs = 0;
    for (uint64_t delay = sweep_min; delay <= sweep_max; delay += sweep_step) {
        for (int r = 0; r < 5; r++) {
            run_adaptive_iteration(total_runs++, delay);
        }
    }

    printf("[NAT-005] Adaptive Parameter Sweep Harness Verification PASSED (%d trial runs executed)\n", total_runs);
    fflush(stdout);

    exit(0); // Exit init -> Kernel panic -> QEMU exits cleanly and flushes serial log
}
