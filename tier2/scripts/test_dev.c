#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main() {
    int fd = open("/proc/devices", O_RDONLY);
    char buf[4096];
    int n = read(fd, buf, sizeof(buf)-1);
    if (n > 0) {
        buf[n] = 0;
        printf("%s\n", buf);
    }
    close(fd);
    return 0;
}
