#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

long read_slabs() {
    char buf[256];
    int fd = open("/sys/kernel/slab/filp/slabs", O_RDONLY);
    if (fd < 0) return -1;
    int n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n > 0) {
        buf[n] = 0;
        return strtol(buf, NULL, 10);
    }
    return -1;
}

int main() {
    printf("[*] ROOT read: %ld slabs\n", read_slabs());
    
    if (setresuid(2000, 2000, 2000) != 0) {
        perror("setresuid");
        return 1;
    }
    
    printf("[*] UID 2000 read: %ld slabs\n", read_slabs());
    
    return 0;
}
