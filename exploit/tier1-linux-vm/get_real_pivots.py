import re
with open("/tmp/rsp_rdi.txt") as f:
    lines = f.readlines()
for i, line in enumerate(lines):
    if "mov    %rdi,%rsp" in line:
        if i + 1 < len(lines):
            next_line = lines[i+1].strip()
            if next_line.endswith("ret") or next_line.endswith("retq"):
                print(line.strip())
                print(next_line)
                print("---")
