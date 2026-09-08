#include <stdio.h>
#include <stdint.h>

struct dma_heap_allocation_data {
    uint64_t len;
    uint32_t fd;
    uint32_t fd_flags;
    uint64_t heap_flags;
};

int main() {
    printf("size: %lu\n", sizeof(struct dma_heap_allocation_data));
    return 0;
}
