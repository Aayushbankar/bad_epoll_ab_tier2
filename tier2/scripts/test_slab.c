#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <sys/resource.h>
#include <string.h>
#include <errno.h>

int main() {
    struct rlimit rl;
    rl.rlim_cur = 200000;
    rl.rlim_max = 200000;
    setrlimit(RLIMIT_NOFILE, &rl);

    int fd = open("/sys/kernel/slab/filp/slabs", O_RDONLY);
    char buf[64] = {0};
    int n = read(fd, buf, sizeof(buf)-1);
    buf[n] = 0;
    printf("Initial slabs: %s\\n", buf);
    close(fd);
    
    int fds[4000];
    int count = 0;
    for (int i=0; i<4000; i++) {
        fds[i] = open("/dev/null", O_RDONLY);
        if (fds[i] >= 0) count++;
    }
    printf("Successfully opened: %d\\n", count);
    
    fd = open("/sys/kernel/slab/filp/slabs", O_RDONLY);
    n = read(fd, buf, sizeof(buf)-1);
    buf[n] = 0;
    printf("Peak slabs: %s\\n", buf);
    close(fd);
    return 0;
}
