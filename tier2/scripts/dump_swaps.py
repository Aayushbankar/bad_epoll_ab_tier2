import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")
try:
    gdb.execute("target remote :1234")
    for i in range(-0x60, 0x60, 8):
        val = int(gdb.parse_and_eval(f"*(unsigned long long*)((char*)&swaps_proc_ops + {i})")) & 0xffffffffffffffff
        print(f"swaps_proc_ops{i:+d} (0x{i+0x60:x}): {hex(val)}")
except Exception as e:
    print(e)
gdb.execute("quit")
