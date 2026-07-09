import re

with open("/tmp/ropgadget.txt") as f:
    for line in f:
        line = line.strip()
        if "rsp" in line and "rdi" in line:
            if line.endswith("ret") or line.endswith("retq"):
                print(line)
