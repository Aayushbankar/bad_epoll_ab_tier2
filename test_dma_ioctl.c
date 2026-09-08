#include <stdio.h>
#include <linux/dma-heap.h>

int main() {
    printf("ioctl: 0x%lx\n", DMA_HEAP_IOCTL_ALLOC);
    return 0;
}
