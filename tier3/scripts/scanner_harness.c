#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

int main() {
    printf("[HARNESS] PID: %d\n", getpid());
    int fd = memfd_create("test", 0);
    int fd2 = dup(fd);
    int fd3 = dup(fd);
    int swap_fd = open("/proc/swaps", O_RDONLY);
    
    sleep(15); int epfd = epoll_create(1);
    struct epoll_event ev; ev.events = EPOLLIN; ev.data.u64 = 0xdeadbeef;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    
    int iteration = 0;
    while(1) {
        if (iteration == 5) {
            printf("[HARNESS] Closing fd3\n");
            close(fd3);
        }
        epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
        sleep(1);
        iteration++;
    }
    return 0;
}
