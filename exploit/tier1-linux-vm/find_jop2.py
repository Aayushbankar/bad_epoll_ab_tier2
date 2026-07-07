import re
with open("/tmp/ropgadget.txt") as f:
    lines = f.readlines()
for line in lines:
    if "mov rsp" in line and "call" in line:
        print(line.strip())
    if "mov rsp" in line and "jmp" in line:
        print(line.strip())
