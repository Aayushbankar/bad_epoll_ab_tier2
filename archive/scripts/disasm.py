from pwn import *
context.arch = 'amd64'
code = bytes.fromhex("8d 96 00 10 00 00 4c 01 80 f0 0f 00 00 48 8d 5e 63 4c 01 80 f8 0f 00 00 48 8d 05 4e 2e c3 01 4c 01 80 d8 0f 00 00 4c 01 80 d0 0f 00 00 48 8d 05 3d 3e 43 02 c7 00 02 00 00 00 84 d2 0f 85 e6 01")
print(disasm(code))
