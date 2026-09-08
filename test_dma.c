#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>

#define DMA_HEAP_IOCTL_ALLOC 0xc0184800
struct dma_heap_allocation_data {
    uint64_t len;
    uint32_t fd;
    uint32_t fd_flags;
    uint64_t heap_flags;
};

int main() {
    int fd = open("/dev/dma_heap/system", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    struct dma_heap_allocation_data alloc = { .len = 4096, .fd_flags = O_RDWR | O_CLOEXEC, .heap_flags = 0 };
    int ret = ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
    if (ret < 0) { perror("ioctl"); return 1; }
    
    printf("success, fd=%d\n", alloc.fd);
    return 0;
}
