import subprocess
import re

print("Running objdump...")
proc = subprocess.Popen(
    ['objdump', '-d', '-M', 'intel', 'linux-6.12.67/vmlinux'],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)

window = []
for line in proc.stdout:
    line = line.strip()
    if not line: continue
    
    # Simple regex to remove the address and hex bytes
    # e.g., ffffffff81000000: 48 89 c7 mov rdi,rax -> mov rdi,rax
    m = re.search(r':\s*[0-9a-f ]+\s+([^\n]+)', line)
    if not m: continue
    insn = m.group(1).strip()
    
    window.append((line, insn))
    if len(window) > 4:
        window.pop(0)
        
    if len(window) >= 2:
        # push rdi ; pop rsp
        if 'push   rdi' in window[-2][1] and 'pop    rsp' in window[-1][1]:
            print(f"Found pivot:\n{window[-2][0]}\n{window[-1][0]}\n")
        # mov rsp, rdi
        if 'mov    rsp,rdi' in window[-1][1]:
            print(f"Found mov rsp, rdi:\n{window[-1][0]}\n")
        # xchg rsp, rdi
        if 'xchg   rsp,rdi' in window[-1][1] or 'xchg   rdi,rsp' in window[-1][1]:
            print(f"Found xchg rsp, rdi:\n{window[-1][0]}\n")

proc.stdout.close()
proc.wait()
