import sys
import re

with open("/tmp/rsp_rdi.txt") as f:
    text = f.read()

lines = text.split('\n')
window = []
for line in lines:
    if not line: continue
    
    m = re.search(r':\s*[0-9a-f ]+\s+([^\n]+)', line)
    if not m: continue
    insn = m.group(1).strip()
    
    window.append((line, insn))
    if len(window) > 4:
        window.pop(0)
        
    if 'ret' in window[-1][1] or 'jmp' in window[-1][1] or 'call' in window[-1][1]:
        if any('mov    %rsp,%rdi' in w[1] or 'mov    %rsp,%rdx' in w[1] for w in window):
            for w in window:
                print(w[0])
            print("---")
        elif any('pop    %rsp' in w[1] for w in window):
            if any('push   %rdi' in w[1] or 'push   %rdx' in w[1] for w in window):
                for w in window:
                    print(w[0])
                print("---")
