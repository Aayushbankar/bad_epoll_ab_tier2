import re

with open("tier2/scripts/test_hyp012_swaps.c", "r") as f:
    code = f.read()

new_drain = """    for (int i=0; i<256; i++) close(batch_a[i]);
    for (int i=0; i<256; i++) close(batch_b[i]);
    printf("[*] Slabs drained. Waiting RCU grace...\\n");
    sleep(2);
"""

code = re.sub(r"    for \(int i=0; i<256; i\+\+\) close\(batch_a\[i\]\);.*?    sleep\(2\);", new_drain, code, flags=re.DOTALL)

with open("tier2/scripts/test_hyp012_swaps.c", "w") as f:
    f.write(code)
