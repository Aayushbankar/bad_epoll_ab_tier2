import sys

pivots = [
    "push rdi ; pop rsp ; ret",
    "mov rsp, rdi ; ret",
    "xchg rsp, rdi ; ret",
    "xchg rdi, rsp ; ret",
    "push rdx ; pop rsp ; ret",
    "mov rsp, rdx ; ret",
    "xchg rsp, rdx ; ret",
    "xchg rdx, rsp ; ret",
]

found_pivots = set()
pop_rdi_ret = None
pop_ret = None

print("Scanning ROPgadget output...")
try:
    with open("/tmp/ropgadget.txt", "r") as f:
        for line in f:
            line = line.strip()
            
            # Simple substring matches first for speed
            if "ret" not in line:
                continue
                
            for p in pivots:
                if p in line:
                    if line not in found_pivots:
                        found_pivots.add(line)
                        print(f"PIVOT: {line}")
            
            if "pop rdi ; ret" in line and not pop_rdi_ret:
                pop_rdi_ret = line
                print(f"POP_RDI_RET: {line}")
                
            if "pop rdi" not in line and line.endswith(": ret") and not pop_ret:
                pop_ret = line
                print(f"POP_RET: {line}")
except Exception as e:
    print(f"Error: {e}")

print("Done.")
