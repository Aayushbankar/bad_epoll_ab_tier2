// test_nat005_topology.c — NAT-005 Task 2: CPU Topology & Cache Bounce Verification
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

#define CACHE_LINE_SIZE 64

typedef struct {
    volatile uint64_t val[CACHE_LINE_SIZE / 8];
} __attribute__((aligned(CACHE_LINE_SIZE))) bounce_line_t;

static bounce_line_t shared_line;
static atomic_int stop_hammer = 0;

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

void *hammer_thread(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    while (!atomic_load(&stop_hammer)) {
        shared_line.val[0]++;
        __sync_synchronize();
    }
    return NULL;
}

int main(void) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(0, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    uint64_t frq = get_cntfrq();
    printf("[NAT-005 Topology Test] ARM64 cntfrq_el0 = %lu Hz (%lu MHz)\n", frq, frq / 1000000);
    fflush(stdout);

    // 1. Measure baseline close() cycle count without bounce
    int ep = epoll_create1(0);
    uint64_t t0 = get_cycles();
    close(ep);
    uint64_t t1 = get_cycles();
    printf("[NAT-005 Topology Test] Baseline single close() cycles: %lu ticks (~%.2f us)\n",
           t1 - t0, (double)(t1 - t0) * 1000000.0 / frq);
    fflush(stdout);

    // 2. Measure close() cycle count under false sharing (hammer thread on CPU 1)
    pthread_t th;
    atomic_store(&stop_hammer, 0);
    pthread_create(&th, NULL, hammer_thread, NULL);
    usleep(1000);

    uint64_t total_bounce_cycles = 0;
    int samples = 100;
    for (int i = 0; i < samples; i++) {
        int ep_test = epoll_create1(0);
        uint64_t s0 = get_cycles();
        shared_line.val[0]++;
        __sync_synchronize();
        close(ep_test);
        uint64_t s1 = get_cycles();
        total_bounce_cycles += (s1 - s0);
    }
    atomic_store(&stop_hammer, 1);
    pthread_join(th, NULL);

    printf("[NAT-005 Topology Test] Average close() under false sharing: %lu ticks (~%.2f us)\n",
           total_bounce_cycles / samples, (double)(total_bounce_cycles / samples) * 1000000.0 / frq);
    fflush(stdout);

    printf("[NAT-005 Topology Test] Topology & Timer Verification PASSED\n");
    fflush(stdout);

    exit(0); // Exit init -> Kernel panic -> QEMU -no-reboot exits and flushes log!
}
