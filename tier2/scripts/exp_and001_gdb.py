import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")

gdb.execute("set logging file /mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/evidence/AND-001_raw_ipc.log")
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

class LoadMsgBreak(gdb.Breakpoint):
    def __init__(self):
        super(LoadMsgBreak, self).__init__("load_msg", internal=False)

    def stop(self):
        src = int(gdb.parse_and_eval("$x0"))
        len_val = int(gdb.parse_and_eval("$x1"))
        log(f"RUNTIME BREAKPOINT HIT: load_msg(src={hex(src)}, len={len_val})")
        log("SysV IPC msgsnd syscall successfully trapped in kernel!")
        self.enabled = False
        return False

LoadMsgBreak()
log("Breakpoint set on load_msg symbol. Continuing execution...")
gdb.execute("continue")
