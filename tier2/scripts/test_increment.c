#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <string.h>

int main(int argc, char **argv) {
    int tmp_epA = epoll_create1(0);
    int tmp_epB = epoll_create1(0);
    struct epoll_event ev = {0};
    epoll_ctl(tmp_epA, EPOLL_CTL_ADD, tmp_epB, &ev);
    close(tmp_epA);
    close(tmp_epB);
    
    char buf[128];
    int fd = open("/sys/kernel/debug/epoll_uaf/fep_cleared", O_RDONLY);
    if (fd >= 0) {
        int n = read(fd, buf, sizeof(buf)-1);
        buf[n] = 0;
        printf("fep_cleared = %s\n", buf);
        close(fd);
    }
    return 0;
}
