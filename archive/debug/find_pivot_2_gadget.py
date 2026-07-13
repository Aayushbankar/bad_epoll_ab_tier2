import re

print("Searching for 2-gadget pivot...")
with open("/tmp/ropgadget.txt") as f:
    for line in f:
        line = line.strip()
        # Gadget 1: something that uses rdx, e.g. push rdx, mov rdi, rdx
        if "mov rdi, rdx" in line or "push rdx" in line:
            if "jmp" in line or "call" in line:
                print(f"Gadget 1 candidates: {line}")
        # Gadget 2: push rdi ; pop rsp ; ret or similar
        if "push rdi ; pop rsp" in line or "mov rsp, rdi" in line:
            if line.endswith("ret"):
                print(f"Gadget 2 candidates: {line}")
