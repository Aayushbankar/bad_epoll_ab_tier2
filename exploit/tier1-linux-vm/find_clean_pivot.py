import re

print("Searching for usable stack pivots (no offset > 0x1000)...")
with open("/tmp/ropgadget.txt") as f:
    for line in f:
        line = line.strip()
        # Look for push rdx ; pop rsp ; ret or mov rsp, rdx ; ret etc
        if "ret" in line and not re.search(r'ret 0x[1-9a-f][0-9a-f]{3,}', line):
            if "mov rsp, rdx" in line or "push rdx ; pop rsp" in line or "xchg rsp, rdx" in line or "xchg rdx, rsp" in line:
                print(line)
            if "mov rsp, rdi" in line or "push rdi ; pop rsp" in line or "xchg rsp, rdi" in line or "xchg rdi, rsp" in line:
                print(line)
