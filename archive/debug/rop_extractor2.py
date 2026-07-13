import sys

def parse_code():
    with open("qemu_output.log", "r") as f:
        log = f.read()
    import re
    m = re.search(r'\[\s*([\d\.]+)\]\s*Code:\s*(.*)', log)
    if m:
        return m.group(2)
    return ""

print("Code: " + parse_code())
