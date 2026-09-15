#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <string.h>

volatile int wait1 = 1;
volatile int wait2 = 1;
volatile int wait3 = 1;

unsigned long get_kallsyms(const char *name) {
    FILE *f = fopen("/proc/kallsyms", "r");
    if (!f) return 0;
    char line[256];
    unsigned long addr = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, name)) {
            char *p = strtok(line, " ");
            if (p) {
                addr = strtoul(p, NULL, 16);
                break;
            }
        }
    }
    fclose(f);
    return addr;
}

int main() {
    unsigned long init_task = get_kallsyms(" init_task\n");
    unsigned long swaps_poll = get_kallsyms(" swaps_poll\n");
    printf("KALLSYMS: init_task=%lx swaps_poll=%lx\n", init_task, swaps_poll);
    
    int fd = open("/dummy", O_CREAT | O_RDWR, 0666);
    int fd2 = dup(fd);
    int fd3 = dup(fd);
    
    printf("MARKER1: wait1=%p fd=%d\n", (void*)&wait1, fd);
    fflush(stdout);
    while (wait1) { volatile int i = 0; i++; }
    
    close(fd3);
    printf("MARKER2: wait2=%p\n", (void*)&wait2);
    fflush(stdout);
    while (wait2) { volatile int i = 0; i++; }
    
    int epfd = epoll_create(1);
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    
    int swapfd = open("/proc/cmdline", O_RDONLY);
    
    printf("MARKER3: wait3=%p epfd=%d swapfd=%d\n", (void*)&wait3, epfd, swapfd);
    fflush(stdout);
    while (wait3) { volatile int i = 0; i++; }
    
    return 0;
}
