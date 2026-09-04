import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")

def log(msg):
    print(f"[*] {msg}", flush=True)

for i in range(15):
    try:
        gdb.execute("target remote :1234")
        log("Connected to QEMU via GDB :1234")
        break
    except gdb.error:
        time.sleep(1)

log("Starting execution without interrupting (script running)...")

def dump_and_quit():
    time.sleep(240) # Wait 4 minutes
    log("Time elapsed. Exiting GDB.")
    import os
    import signal
    os.kill(os.getpid(), signal.SIGINT)

import threading
t = threading.Thread(target=dump_and_quit)
t.daemon = True
t.start()

try:
    gdb.execute("continue")
except Exception as e:
    log("GDB interrupted.")

gdb.execute("quit")
