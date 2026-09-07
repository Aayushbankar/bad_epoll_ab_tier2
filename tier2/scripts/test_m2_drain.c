#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/eventfd.h>
#include <sys/resource.h>

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

#define NUM_FILES 4000

int main() {
    struct rlimit rl;
    rl.rlim_cur = 65535;
    rl.rlim_max = 65535;
    if (setrlimit(RLIMIT_NOFILE, &rl) != 0) {
        perror("setrlimit");
    }

    if (setresuid(2000, 2000, 2000) != 0) {
        perror("setresuid");
        return 1;
    }
    
    printf("[*] Dropped to UID 2000. Allocating %d files...\n", NUM_FILES);
    int fds[NUM_FILES];
    for (int i = 0; i < NUM_FILES; i++) {
        fds[i] = eventfd(0, 0);
        if (fds[i] < 0) {
            perror("eventfd");
            break;
        }
    }
    
    long slabs_before = read_slabs();
    printf("[*] Slabs before drain: %ld\n", slabs_before);
    
    printf("[*] Closing files...\n");
    for (int i = 0; i < NUM_FILES; i++) {
        if (fds[i] >= 0) close(fds[i]);
    }
    
    printf("[*] Waiting for RCU grace period (2s)...\n");
    sleep(2);
    
    long slabs_after = read_slabs();
    printf("[*] Slabs after drain: %ld\n", slabs_after);
    printf("[*] Telemetry delta: %ld -> %ld slabs.\n", slabs_before, slabs_after);
    
    return 0;
}
