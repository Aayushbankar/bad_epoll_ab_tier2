import re
with open("/tmp/ropgadget.txt") as f:
    lines = f.readlines()
for line in lines:
    if "mov rsp" in line and "jmp" in line:
        print(line.strip())
    if "push rdi" in line and "pop rsp" in line:
        print(line.strip())
    if "mov rax, qword ptr [rdx" in line and "jmp" in line:
        print(line.strip())
