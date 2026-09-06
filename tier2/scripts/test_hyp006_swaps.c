// test_hyp006_swaps.c — HYP-006: Verify swaps_poll write gadget
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <string.h>

int main(void) {
    printf("=== HYP-006: Verify swaps_poll Gadget ===\n");
    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);

    int fd = open("/proc/swaps", O_RDONLY);
    if (fd < 0) {
        perror("[-] Failed to open /proc/swaps");
    } else {
        printf("[+] /proc/swaps opened successfully (fd=%d)\n", fd);
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        int ret = poll(&pfd, 1, 100);
        printf("[+] poll(/proc/swaps) returned: %d, revents=0x%x (POLLIN=0x%x)\n",
               ret, pfd.revents, POLLIN);
        if (pfd.revents & POLLIN) {
            printf("[+] HYP-006 CONFIRMED: swaps_poll is active, compiled-in, and reachable via poll()!\n");
        }
        close(fd);
    }

    reboot(RB_POWER_OFF);
    return 0;
}
