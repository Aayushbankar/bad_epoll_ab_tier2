#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
int main() {
    char buf[128];
    int fd = open("/sys/kernel/slab/filp/object_size", O_RDONLY);
    int n = read(fd, buf, 127);
    buf[n] = 0;
    printf("object_size: %s", buf);
    close(fd);
    return 0;
}
