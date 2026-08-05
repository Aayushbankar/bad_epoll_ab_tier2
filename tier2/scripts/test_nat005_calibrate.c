// test_nat005_calibrate.c — Precise empirical measurement of close() execution phases & hlist_del_rcu write offset
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/reboot.h>
#include <string.h>

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

int main(void) {
    char buf[384];
    uint64_t frq = get_cntfrq();
    snprintf(buf, sizeof(buf), "[CALIBRATE] Empirical Cycle Benchmark Starting (ARM64 freq: %lu Hz)\n", frq);
    print_msg(buf);

    // Warmup cycle counter
    get_cycles();

    // 1. Measure empty epoll close()
    uint64_t total_empty = 0;
    for (int i = 0; i < 100; i++) {
        int ep = epoll_create1(0);
        uint64_t t0 = get_cycles();
        close(ep);
        uint64_t t1 = get_cycles();
        total_empty += (t1 - t0);
    }
    uint64_t avg_empty = total_empty / 100;

    // 2. Measure 1-item outer epoll close()
    uint64_t total_1item = 0;
    struct epoll_event ev = { .events = EPOLLIN };
    for (int i = 0; i < 100; i++) {
        int ep = epoll_create1(0);
        int in0 = epoll_create1(0);
        ev.data.fd = in0;
        epoll_ctl(ep, EPOLL_CTL_ADD, in0, &ev);
        uint64_t t0 = get_cycles();
        close(ep);
        uint64_t t1 = get_cycles();
        total_1item += (t1 - t0);
        close(in0);
    }
    uint64_t avg_1item = total_1item / 100;

    // 3. Measure 2-item outer epoll close() (NAT-005 race target)
    uint64_t total_2item = 0;
    for (int i = 0; i < 100; i++) {
        int ep = epoll_create1(0);
        int in0 = epoll_create1(0);
        int in1 = epoll_create1(0);
        ev.data.fd = in0; epoll_ctl(ep, EPOLL_CTL_ADD, in0, &ev);
        ev.data.fd = in1; epoll_ctl(ep, EPOLL_CTL_ADD, in1, &ev);
        uint64_t t0 = get_cycles();
        close(ep);
        uint64_t t1 = get_cycles();
        total_2item += (t1 - t0);
        close(in0); close(in1);
    }
    uint64_t avg_2item = total_2item / 100;

    snprintf(buf, sizeof(buf),
             "[CALIBRATE] Empirical Results (100-sample averages):\n"
             "  - Baseline empty close(ep): %lu cycles\n"
             "  - 1-item close(ep_outer): %lu cycles\n"
             "  - 2-item close(ep_outer): %lu cycles\n"
             "  - Per-item __ep_remove overhead: %lu cycles\n"
             "  - Measured Critical Window Target Offset for 2nd item (ep_inner1): %lu cycles\n",
             avg_empty, avg_1item, avg_2item,
             avg_2item > avg_1item ? avg_2item - avg_1item : 150,
             avg_1item + (avg_2item > avg_1item ? (avg_2item - avg_1item) / 2 : 75));
    print_msg(buf);

    reboot(RB_POWER_OFF);
    return 0;
}
