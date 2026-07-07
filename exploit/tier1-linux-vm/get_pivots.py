import re
with open("/tmp/rsp_rdi.txt") as f:
    lines = f.readlines()
for i, line in enumerate(lines):
    if "mov    %rdi,%rsp" in line:
        if i + 1 < len(lines):
            next_line = lines[i+1].strip()
            if "ret" in next_line or "jmp" in next_line:
                print(line.strip())
                print(next_line)
                print("---")
