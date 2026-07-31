#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>

int main() {
    printf("[*] Starting register liveness trace...\n");
    
    int ep_outer = epoll_create1(0);
    int ep_inner = epoll_create1(0);
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = ep_inner;
    epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner, &ev);
    
    printf("[*] Closing inner_epoll...\n");
    close(ep_inner); // triggers eventpoll_release_file
    
    printf("[*] Done.\n");
    return 0;
}
