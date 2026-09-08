import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")

for i in range(30):
    try:
        gdb.execute("target remote :1234")
        print("Connected!", flush=True)
        break
    except Exception:
        time.sleep(1)

try:
    for i in range(0, 8000, 8):
        try:
            val = gdb.parse_and_eval(f"*(unsigned long long*)((char*)&init_task + {i})")
            val = int(val) & 0xffffffffffffffff
            if val != 0:
                print(f"offset {hex(i)}: {hex(val)}")
        except:
            pass
except Exception as e:
    print(e)
gdb.execute("quit")
