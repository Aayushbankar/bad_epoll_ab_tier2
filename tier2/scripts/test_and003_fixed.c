#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/stat.h>

void __attribute__((noinline)) my_print(const char *msg) {
    syscall(SYS_write, 1, msg, strlen(msg));
}

void trigger_syscalls() {
    int boot_fd = open("/proc/cmdline", O_RDONLY);
    if (boot_fd >= 0) {
        char buf[1024] = {0};
        read(boot_fd, buf, sizeof(buf)-1);
        if (strstr(buf, "enforcing=1")) {
            my_print("[AND-003] SELinux Mode confirmed via cmdline: 1 (Enforcing)\n");
        } else {
            my_print("[AND-003] SELinux Mode confirmed via cmdline: 0 (Permissive)\n");
        }
        close(boot_fd);
    } else {
        my_print("[AND-003] FAIL: Could not determine SELinux mode\n");
    }
    
    int epfd = epoll_create1(0);
    if (epfd < 0) {
        my_print("[AND-003] FAIL: epoll_create1 failed\n");
    } else {
        my_print("[AND-003] PASS: epoll_create1 success\n");
        close(epfd);
    }
    
    int msqid = msgget(IPC_PRIVATE, 0666 | IPC_CREAT);
    if (msqid < 0) {
        my_print("[AND-003] FAIL: msgget failed\n");
    } else {
        my_print("[AND-003] PASS: msgget success\n");
        msgctl(msqid, IPC_RMID, NULL);
    }
    
    my_print("[AND-003] Audit Complete\n");
}

int main() {
    my_print("[AND-003] SELinux Enforcing Syscall Audit\n");
    trigger_syscalls();
    return 0;
}
