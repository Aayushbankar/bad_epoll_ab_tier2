import subprocess
import re

print("Running objdump to find stack pivots...")
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
    if len(window) > 3:
        window.pop(0)
        
    if len(window) >= 1:
        if ('ret' in window[-1][1] or 'jmp' in window[-1][1] or 'call' in window[-1][1]):
            # Look for mov rsp, xxx in the window
            for i, w in enumerate(window[:-1]):
                if re.match(r'(mov|lea|xchg)\s+rsp,\s*(rdi|rdx|rax|rcx|rbx|rsi|rbp|r8|r9|r10|r11|r12|r13|r14|r15)', w[1]):
                    for j in range(i, len(window)):
                        print(window[j][0])
                    print("---")
            # Look for pop rsp
            for i, w in enumerate(window[:-1]):
                if 'pop    rsp' in w[1]:
                    for j in range(0, len(window)):
                        print(window[j][0])
                    print("---")

proc.stdout.close()
proc.wait()
