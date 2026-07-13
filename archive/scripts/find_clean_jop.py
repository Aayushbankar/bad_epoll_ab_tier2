import re

with open("/tmp/ropgadget.txt") as f:
    for line in f:
        line = line.strip()
        # mov rax, [rdx + something] -> call rax
        if "call rax" in line or "jmp rax" in line:
            if "mov rsp" in line or "pop rsp" in line:
                print(line)
        if "call" in line or "jmp" in line:
            if "xchg rsp, rdx" in line or "xchg rdx, rsp" in line:
                print(line)
