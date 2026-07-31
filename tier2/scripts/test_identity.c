#include <unistd.h>
#include <sys/epoll.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <stdio.h>

int main() {
    write(1, "[*] Identity Test Started\n", 26);
    
    int epfd = syscall(SYS_epoll_create1, 0);
    int tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = tfd;
    
    syscall(SYS_epoll_ctl, epfd, EPOLL_CTL_ADD, tfd, &ev);
    
    // This close will trigger ep_clear_and_put(epfd)
    // which will drain the waitqueue and free the epi
    syscall(SYS_close, epfd);
    
    write(1, "[*] Test Done\n", 14);
    
    // Prevent immediate exit so GDB can catch up
    while(1) { sleep(1); }
    return 0;
}
