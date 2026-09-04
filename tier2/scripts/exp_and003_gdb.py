import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")

gdb.execute("set logging file evidence/AND-003_raw_enforcing.log")
gdb.execute("set logging overwrite on")
gdb.execute("set logging enabled on")

def log(msg):
    print(f"[*] {msg}", flush=True)

def connect():
    for i in range(10):
        try:
            gdb.execute("target remote :1234")
            log("Connected to QEMU")
            return True
        except gdb.error as e:
            log(f"Connection failed: {e}, retrying...")
            time.sleep(1)
    return False

if not connect():
    log("Failed to connect to QEMU port :1234")
    gdb.execute("quit")

# We want to dump kernel dmesg
class HaltBreak(gdb.Breakpoint):
    def __init__(self):
        super(HaltBreak, self).__init__("__arm64_sys_getpid", internal=False)

    def stop(self):
        log("Harness finished, dumping kernel log buffer...")
        gdb.execute("lx-dmesg")
        gdb.execute("quit")
        return False

HaltBreak()
gdb.execute("continue")
