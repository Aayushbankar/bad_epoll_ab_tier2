#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <linux/reboot.h>

#define SYS_epoll_create1 20
#define SYS_epoll_ctl 21
#define SYS_close 57
#define SYS_unshare 97

int outer_epoll;
int inner_epoll;

void *thread_a(void *arg) {
    write(1, "[*] Thread A running\n", 21);
    syscall(SYS_unshare, 1111);
    syscall(SYS_close, outer_epoll);
    return NULL;
}

void *thread_b(void *arg) {
    write(1, "[*] Thread B running\n", 21);
    syscall(SYS_unshare, 2222);
    syscall(SYS_close, inner_epoll);
    return NULL;
}

int main() {
    write(1, "[*] EXP013 Main Started\n", 24);
    
    outer_epoll = syscall(SYS_epoll_create1, 0);
    inner_epoll = syscall(SYS_epoll_create1, 0);
    
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = inner_epoll;
    
    syscall(SYS_epoll_ctl, outer_epoll, EPOLL_CTL_ADD, inner_epoll, &event);
    
    pthread_t ta, tb;
    int r1 = pthread_create(&ta, NULL, thread_a, NULL);
    int r2 = pthread_create(&tb, NULL, thread_b, NULL);
    
    char buf[64];
    int n = snprintf(buf, sizeof(buf), "[*] pthread_create: %d, %d\n", r1, r2);
    write(1, buf, n);
    
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    
    write(1, "[*] Test Done\n", 14);
    return 0;
}
