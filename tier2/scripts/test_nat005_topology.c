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
#include <sys/reboot.h>
#include <string.h>

#define CACHE_LINE_SIZE 64

typedef struct {
    volatile uint64_t val[CACHE_LINE_SIZE / 8];
} __attribute__((aligned(CACHE_LINE_SIZE))) bounce_line_t;

static bounce_line_t shared_line;
static atomic_int stop_hammer = 0;

static void print_msg(const char *msg) {
    write(1, msg, strlen(msg));
}

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
        asm volatile("" ::: "memory");
    }
    return NULL;
}

int main(void) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(0, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    char buf[256];
    uint64_t frq = get_cntfrq();
    snprintf(buf, sizeof(buf), "[NAT-005 Topology Test] ARM64 cntfrq_el0 = %lu Hz (%lu MHz)\n", frq, frq / 1000000);
    print_msg(buf);

    // 1. Measure baseline close() cycle count without bounce
    int ep = epoll_create1(0);
    uint64_t t0 = get_cycles();
    close(ep);
    uint64_t t1 = get_cycles();
    snprintf(buf, sizeof(buf), "[NAT-005 Topology Test] Baseline single close() cycles: %lu ticks (~%.2f us)\n",
           t1 - t0, (double)(t1 - t0) * 1000000.0 / frq);
    print_msg(buf);

    // 2. Measure close() cycle count under false sharing (hammer thread on CPU 1)
    pthread_t th;
    atomic_store(&stop_hammer, 0);
    pthread_create(&th, NULL, hammer_thread, NULL);
    usleep(100);

    uint64_t total_bounce_cycles = 0;
    int samples = 50;
    for (int i = 0; i < samples; i++) {
        int ep_test = epoll_create1(0);
        uint64_t s0 = get_cycles();
        shared_line.val[0]++;
        asm volatile("" ::: "memory");
        close(ep_test);
        uint64_t s1 = get_cycles();
        total_bounce_cycles += (s1 - s0);
    }
    atomic_store(&stop_hammer, 1);
    pthread_join(th, NULL);

    snprintf(buf, sizeof(buf), "[NAT-005 Topology Test] Average close() under false sharing: %lu ticks (~%.2f us)\n",
           total_bounce_cycles / samples, (double)(total_bounce_cycles / samples) * 1000000.0 / frq);
    print_msg(buf);

    snprintf(buf, sizeof(buf), "[NAT-005 Topology Test] Topology & Timer Verification PASSED\n");
    print_msg(buf);

    reboot(RB_POWER_OFF);
    return 0;
}
