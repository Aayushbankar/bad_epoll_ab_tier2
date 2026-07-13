import re
with open("/tmp/ropgadget.txt") as f:
    lines = f.readlines()
for line in lines:
    if "mov rsp, rdi ; ret" in line or "push rdi ; pop rsp ; ret" in line:
        print(line.strip())
