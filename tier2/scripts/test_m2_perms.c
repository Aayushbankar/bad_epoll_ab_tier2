#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/sysmacros.h>
#include <string.h>

int main() {
    mkdir("/sys", 0755);
    mkdir("/dev", 0755);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    
    // Create the device node with standard permissions (as udev would)
    mkdir("/dev/dma_heap", 0755);
    mknod("/dev/dma_heap/system", S_IFCHR | 0600, makedev(254, 0));
    
    sleep(1);

    printf("[*] ROOT CHECK:\n");
    
    struct stat st1, st2;
    if (stat("/sys/kernel/slab/filp/slabs", &st1) == 0) {
        printf("/sys/kernel/slab/filp/slabs mode: %o uid: %d gid: %d\n", st1.st_mode & 0777, st1.st_uid, st1.st_gid);
    } else {
        perror("stat slabs");
    }
    
    if (stat("/dev/dma_heap/system", &st2) == 0) {
        printf("/dev/dma_heap/system mode: %o uid: %d gid: %d\n", st2.st_mode & 0777, st2.st_uid, st2.st_gid);
    } else {
        perror("stat dma_heap");
    }
    
    printf("\n[*] Dropping privileges to UID 2000...\n");
    if (setresuid(2000, 2000, 2000) != 0) {
        perror("setresuid");
        return 1;
    }
    
    printf("[*] UID 2000 CHECK:\n");
    int fd1 = open("/sys/kernel/slab/filp/slabs", O_RDONLY);
    if (fd1 < 0) {
        perror("open /sys/kernel/slab/filp/slabs");
    } else {
        printf("Successfully opened /sys/kernel/slab/filp/slabs\n");
        close(fd1);
    }
    
    int fd2 = open("/dev/dma_heap/system", O_RDWR);
    if (fd2 < 0) {
        perror("open /dev/dma_heap/system");
    } else {
        printf("Successfully opened /dev/dma_heap/system\n");
        close(fd2);
    }
    
    return 0;
}
