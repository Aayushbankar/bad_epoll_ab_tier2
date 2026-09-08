import gdb
import sys
import time

def log(msg):
    print(f"[DUMP] {msg}", flush=True)
    sys.stdout.flush()

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

connected = False
for i in range(10):
    try:
        gdb.execute("target remote :1234")
        log("Connected to QEMU on :1234")
        connected = True
        break
    except gdb.error as e:
        log(f"Retry {i}: {e}")
        time.sleep(1)

if not connected:
    log("Failed to connect")
    sys.exit(1)

try:
    init_task_addr = int(gdb.parse_and_eval("&init_task")) & 0xffffffffffffffff
    log(f"init_task @ {hex(init_task_addr)}")
    
    # scan for "swapper" in init_task
    for offset in range(0, 0x1000, 8):
        try:
            val = int(gdb.parse_and_eval(f"*(unsigned long long*)({init_task_addr} + {offset})")) & 0xffffffffffffffff
            if val == 0x0072657070617773:
                log(f"FOUND 'swapper\\0' at offset {hex(offset)}")
            elif val == 0x2f72657070617773:
                log(f"FOUND 'swapper/' at offset {hex(offset)}")
        except Exception as inner_e:
            log(f"Read error at offset {hex(offset)}: {inner_e}")
            pass
            
except Exception as e:
    log(f"Error: {e}")

gdb.execute("quit")
