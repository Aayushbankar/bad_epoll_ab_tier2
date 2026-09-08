import re
with open("tier2/scripts/exp_hyp012_swaps_gdb.py", "r") as f:
    code = f.read()

# Add the 9999 setup when it hits __fput breakpoint
code = code.replace(
    'gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")',
    'gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")\n            gdb.execute("set *(unsigned int*)0xffffffc00994e920 = 9999")'
)

with open("tier2/scripts/exp_hyp012_swaps_gdb.py", "w") as f:
    f.write(code)
