// test_hyp005_fdinfo.c — HYP-005: Verify ep_show_fdinfo read primitive
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>

int main(void) {
    printf("=== HYP-005: Verify ep_show_fdinfo Read Primitive ===\n");
    mkdir("/proc", 0755);
    mount("proc", "/proc", "proc", 0, NULL);

    int epfd = epoll_create1(0);
    int pfd[2];
    pipe(pfd);

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = pfd[0] };
    epoll_ctl(epfd, EPOLL_CTL_ADD, pfd[0], &ev);

    struct stat st;
    fstat(pfd[0], &st);
    printf("[*] Watched pipe inode ino = 0x%lx (%lu)\n", (unsigned long)st.st_ino, (unsigned long)st.st_ino);

    char path[64];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", epfd);

    // Test reading as unprivileged user (UID 1000)
    pid_t pid = fork();
    if (pid == 0) {
        setgid(1000);
        setuid(1000);
        printf("[*] Dropped to unpriv UID %d\n", getuid());

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("[!] Failed to open fdinfo");
            exit(1);
        }
        char buf[512] = {0};
        int n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        printf("[+] Read from %s (bytes=%d):\n%s\n", path, n, buf);

        if (strstr(buf, "ino:")) {
            printf("[+] HYP-005 CONFIRMED: ep_show_fdinfo exposes inode ino to unprivileged userspace!\n");
        } else {
            printf("[-] HYP-005 REJECTED: ino not found in fdinfo\n");
        }
        exit(0);
    }
    wait(NULL);

    close(epfd);
    close(pfd[0]);
    close(pfd[1]);
    reboot(RB_POWER_OFF);
    return 0;
}
