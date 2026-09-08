#include <sys/mman.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

int main() {
    void *p = mmap((void*)0x7fbade0000, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    if (p == MAP_FAILED) {
        perror("mmap");
        return 1;
    }
    printf("Mapped at %p\n", p);
    // Write something to /proc/sys/kernel/panic to test if kernel can read it?
    // No, we need a kernel dereference of userspace.
    return 0;
}
