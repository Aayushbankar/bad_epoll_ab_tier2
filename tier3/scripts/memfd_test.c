#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
int main() {
    int fd = memfd_create("test", 0);
    if (fd < 0) { perror("memfd_create"); return 1; }
    ftruncate(fd, 4096);
    void *p = mmap(NULL, 4096, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    printf("[+] memfd_create and mmap successful\n");
    return 0;
}
