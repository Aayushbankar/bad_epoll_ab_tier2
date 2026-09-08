import gdb
import time

for i in range(10):
    try:
        gdb.execute("target remote :1234")
        break
    except gdb.error:
        time.sleep(1)

try:
    val1 = gdb.parse_and_eval("*(unsigned long long*)0xffffffc00973e258")
    val2 = gdb.parse_and_eval("*(unsigned long long*)0xffffffc00973e218")
    print(f"0xffffffc00973e258 = {hex(int(val1) & 0xffffffffffffffff)}")
    print(f"0xffffffc00973e218 = {hex(int(val2) & 0xffffffffffffffff)}")
except Exception as e:
    print(e)
gdb.execute("quit")
