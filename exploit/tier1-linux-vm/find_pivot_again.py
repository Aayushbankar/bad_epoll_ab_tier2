import subprocess
import re

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
    
    m = re.search(r':\s*[0-9a-f ]+\s+([^\n]+)', line)
    if not m: continue
    insn = m.group(1).strip()
    
    window.append((line, insn))
    if len(window) > 4:
        window.pop(0)
        
    if len(window) >= 1:
        if ('ret' in window[-1][1] or 'jmp' in window[-1][1] or 'call' in window[-1][1]):
            for i, w in enumerate(window[:-1]):
                if re.match(r'(mov|xchg)\s+rsp,\s*rdi', w[1]) or re.match(r'push\s+rdi', w[1]):
                    if 'push' in w[1]:
                        has_pop = False
                        for j in range(i, len(window)-1):
                            if 'pop    rsp' in window[j][1]:
                                has_pop = True
                                break
                        if not has_pop:
                            continue
                    
                    for j in range(i, len(window)):
                        print(window[j][0])
                    print("---")

proc.stdout.close()
proc.wait()
