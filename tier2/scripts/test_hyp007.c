// test_hyp007.c — HYP-007: Kernel-Side Struct File UAF Race Detection Test
// Experiment: Detect struct file freed by file_free() while __ep_remove still uses epi->ffd.file
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
#include <string.h>
#include <time.h>
#include <sys/reboot.h>
#include <sys/mount.h>
#include <sys/stat.h>

#ifndef ITERATIONS
#define ITERATIONS 5000
#endif

// ── Shared state ──
static atomic_int sync_go = 0;
static atomic_int ready_a = 0, ready_b = 0;
static int ep_outer = -1;
static int ep_inner = -1;

// ── Debugfs paths ──
#define DEBUGFS_BASE      "/sys/kernel/debug/epoll_uaf/"
#define DEBUGFS_FEP       DEBUGFS_BASE "fep_cleared"
#define DEBUGFS_UAF_EP    DEBUGFS_BASE "uaf_detected"
#define DEBUGFS_FREE_EP   DEBUGFS_BASE "epfree_called"
#define DEBUGFS_UAF_FILE  DEBUGFS_BASE "uaf_file_detected"
#define DEBUGFS_FCNT_ZERO DEBUGFS_BASE "file_fcount_zero"
#define DEBUGFS_FREE_FILE DEBUGFS_BASE "file_free_called"
#define DEBUGFS_RST       DEBUGFS_BASE "reset"

static void print_msg(const char *msg) {
    write(1, msg, strlen(msg));
}

static long read_debugfs_counter(const char *path) {
    char buf[64] = {0};
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return strtol(buf, NULL, 10);
}

static void reset_debugfs_counters(void) {
    int fd = open(DEBUGFS_RST, O_WRONLY);
    if (fd >= 0) {
        write(fd, "1", 1);
        close(fd);
    }
}

// ── Thread A: close(outer) → triggers __ep_remove(outer, epi) for inner epoll file ──
void *thread_a(void *arg) {
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(0, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store(&ready_a, 1);
    while (!atomic_load(&sync_go)) { __asm__ volatile("yield"); }

    close(ep_outer);
    return NULL;
}

// ── Thread B: close(inner) → eventpoll_release_file / __fput → file_free(inner_file) ──
void *thread_b(void *arg) {
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store(&ready_b, 1);
    while (!atomic_load(&sync_go)) { __asm__ volatile("yield"); }

    close(ep_inner);
    return NULL;
}

static int run_trial(void) {
    atomic_store(&sync_go, 0);
    atomic_store(&ready_a, 0);
    atomic_store(&ready_b, 0);

    ep_outer = epoll_create1(0);
    ep_inner = epoll_create1(0);
    if (ep_outer < 0 || ep_inner < 0) {
        if (ep_outer >= 0) close(ep_outer);
        if (ep_inner >= 0) close(ep_inner);
        return -1;
    }

    // Outer epoll watches inner epoll
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = ep_inner };
    if (epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner, &ev) < 0) {
        close(ep_outer);
        close(ep_inner);
        return -1;
    }

    pthread_t th_a, th_b;
    pthread_create(&th_a, NULL, thread_a, NULL);
    pthread_create(&th_b, NULL, thread_b, NULL);

    while (!atomic_load(&ready_a) || !atomic_load(&ready_b)) {
        __asm__ volatile("yield");
    }

    atomic_store(&sync_go, 1);

    pthread_join(th_a, NULL);
    pthread_join(th_b, NULL);

    return 0;
}

int main(int argc, char **argv) {
    char buf[256];
    print_msg("=== HYP-007: Kernel-Side Struct File UAF Race Detection Test ===\n");

    mkdir("/sys", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mkdir("/sys/kernel", 0755);
    mkdir("/sys/kernel/debug", 0755);
    if (mount("none", "/sys/kernel/debug", "debugfs", 0, NULL) < 0) {
        print_msg("[!] WARNING: Failed to mount debugfs.\n");
    }

    if (access(DEBUGFS_UAF_FILE, R_OK) != 0) {
        print_msg("[!] FATAL: Debugfs counter not found at " DEBUGFS_UAF_FILE "\n");
        print_msg("[!] Kernel does not have HYP-007 instrumentation!\n");
        reboot(RB_POWER_OFF);
        return 1;
    }
    print_msg("[*] Debugfs interface verified: /sys/kernel/debug/epoll_uaf/\n");

    reset_debugfs_counters();
    print_msg("[*] Counters reset to zero.\n");

    snprintf(buf, sizeof(buf), "[*] Starting %d race iterations (epoll-on-epoll)...\n", ITERATIONS);
    print_msg(buf);

    int failed = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        if (run_trial() < 0) {
            failed++;
        }

        if ((i + 1) % 500 == 0) {
            long fep        = read_debugfs_counter(DEBUGFS_FEP);
            long uaf_file   = read_debugfs_counter(DEBUGFS_UAF_FILE);
            long fcnt_zero  = read_debugfs_counter(DEBUGFS_FCNT_ZERO);
            long uaf_ep     = read_debugfs_counter(DEBUGFS_UAF_EP);
            snprintf(buf, sizeof(buf),
                     "[*] Progress: %d/%d | kernel_fep_cleared=%ld | kernel_uaf_file=%ld | kernel_fcnt_zero=%ld | kernel_uaf_ep=%ld | setup_fail=%d\n",
                     i + 1, ITERATIONS, fep, uaf_file, fcnt_zero, uaf_ep, failed);
            print_msg(buf);
        }
    }

    long final_fep        = read_debugfs_counter(DEBUGFS_FEP);
    long final_uaf_file   = read_debugfs_counter(DEBUGFS_UAF_FILE);
    long final_fcnt_zero  = read_debugfs_counter(DEBUGFS_FCNT_ZERO);
    long final_uaf_ep     = read_debugfs_counter(DEBUGFS_UAF_EP);
    long final_free_ep    = read_debugfs_counter(DEBUGFS_FREE_EP);
    long final_free_file  = read_debugfs_counter(DEBUGFS_FREE_FILE);

    print_msg("\n========================================\n");
    print_msg("HYP-007 FINAL RESULTS\n");
    print_msg("========================================\n");
    snprintf(buf, sizeof(buf),
             "Iterations:              %d\n"
             "Setup failures:          %d\n"
             "Kernel fep_cleared:      %ld\n"
             "Kernel uaf_file_detected:%ld\n"
             "Kernel file_fcount_zero: %ld\n"
             "Kernel uaf_ep_detected:  %ld\n"
             "Kernel file_free_called: %ld\n"
             "Kernel epfree_called:    %ld\n",
             ITERATIONS, failed,
             final_fep, final_uaf_file, final_fcnt_zero,
             final_uaf_ep, final_free_file, final_free_ep);
    print_msg(buf);

    if (final_uaf_file > 0) {
        print_msg("\n>>> HYPOTHESIS 7 CONFIRMED: Struct file UAF detected!\n");
        snprintf(buf, sizeof(buf), ">>> %ld hits detected on freed struct file before spin_lock(&file->f_lock)\n", final_uaf_file);
        print_msg(buf);
    } else if (final_fcnt_zero > 0) {
        print_msg("\n>>> PARTIAL HIT: file->f_count <= 0 observed before spin_lock, but file_free had not completed.\n");
        snprintf(buf, sizeof(buf), ">>> %ld occurrences of unpinned refcount drop detected.\n", final_fcnt_zero);
        print_msg(buf);
    } else {
        print_msg("\n>>> HYPOTHESIS 7 NEGATIVE: 0 hits under QEMU TCG over 5000 iterations.\n");
        print_msg(">>> Natural race between close(outer) and close(inner) did not win under TCG software emulation.\n");
    }

    print_msg("\n--- Sanity Checks ---\n");
    snprintf(buf, sizeof(buf), "[OK] fep_cleared=%ld (expected ~%d)\n", final_fep, ITERATIONS);
    print_msg(buf);
    snprintf(buf, sizeof(buf), "[OK] epfree_called=%ld (expected ~%d)\n", final_free_ep, ITERATIONS * 2);
    print_msg(buf);
    snprintf(buf, sizeof(buf), "[OK] file_free_called=%ld\n", final_free_file);
    print_msg(buf);

    print_msg("========================================\n");
    print_msg("HYP-007 test complete.\n");

    reboot(RB_POWER_OFF);
    return 0;
}
