offsets = [0x1c77810, 0x4ca12d8, 0x75ee487, 0x8ade72f, 0xde21518, 0xe5fe00a]
import subprocess

for offset in offsets:
    addr = f"{0xffffffff81000000 + offset:x}"
    print(f"Offset {hex(offset)} (Addr {addr}):")
    proc = subprocess.run(['objdump', '-d', '-M', 'intel', '--start-address=0x' + addr, '--stop-address=0x' + f"{0xffffffff81000000 + offset + 8:x}", 'linux-6.12.67/vmlinux'], stdout=subprocess.PIPE, text=True)
    for line in proc.stdout.split('\n'):
        if line.strip():
            print(line)
    print("---")
