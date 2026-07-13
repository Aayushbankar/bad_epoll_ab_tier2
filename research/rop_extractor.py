import struct
import sys
import re

with open("qemu_output.log", "r") as f:
    log = f.read()

m = re.search(r'\[\s*([\d\.]+)\]\s*Code:\s*(.*)', log)
if m:
    print(f"Code: {m.group(2)}")
