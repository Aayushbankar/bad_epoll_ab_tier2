#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <linux/dma-heap.h>

int main() {
    int fd = open("/dev/dma_heap/system", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }
    
    struct dma_heap_allocation_data alloc = { .len = 4096, .fd_flags = O_RDWR | O_CLOEXEC, .heap_flags = 0 };
    int ret = ioctl(fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
    if (ret < 0) { perror("ioctl"); return 1; }
    
    printf("success, fd=%d, ioctl=0x%lx\n", alloc.fd, DMA_HEAP_IOCTL_ALLOC);
    return 0;
}
