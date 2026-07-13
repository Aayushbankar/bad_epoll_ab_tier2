import subprocess
import re

print("Running objdump...")
proc = subprocess.Popen(
    ['objdump', '-d', '-M', 'intel', 'linux-6.12.67/vmlinux'],
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)

for line in proc.stdout:
    line = line.strip()
    if not line: continue
    
    if "mov    rsp,rdi" in line:
        print(line)
        continue
        
    if "xchg   rsp,rdi" in line or "xchg   rdi,rsp" in line:
        print(line)
        continue

proc.stdout.close()
proc.wait()
