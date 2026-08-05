import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")

def log(msg):
    print(f"[*] {msg}", flush=True)

for i in range(10):
    try:
        gdb.execute("target remote :1234")
        log("Connected to QEMU")
        break
    except gdb.error as e:
        time.sleep(1)

log("Continuing execution for NAT-005...")
gdb.execute("continue")
