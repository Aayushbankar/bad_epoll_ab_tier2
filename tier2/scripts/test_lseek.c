#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>

int main() {
    int ep = epoll_create1(0);
    char path[256];
    snprintf(path, sizeof(path), "/proc/self/fdinfo/%d", ep);
    int fd = open(path, O_RDONLY);
    char buf[1024];
    int n = read(fd, buf, sizeof(buf)-1);
    if (n > 0) buf[n] = 0; else buf[0] = 0;
    printf("First read:\n%s\n", buf);
    
    lseek(fd, 0, SEEK_SET);
    n = read(fd, buf, sizeof(buf)-1);
    if (n > 0) buf[n] = 0; else buf[0] = 0;
    printf("Second read:\n%s\n", buf);
    return 0;
}
