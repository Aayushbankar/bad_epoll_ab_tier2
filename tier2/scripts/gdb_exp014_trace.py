import gdb
import time

def log(msg):
    print(f"[*] {msg}", flush=True)

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

def connect():
    for i in range(10):
        try:
            gdb.execute("target remote :1234")
            log("Connected to QEMU")
            return True
        except gdb.error as e:
            time.sleep(1)
    return False

if not connect():
    gdb.execute("quit")

class TraceBP(gdb.Breakpoint):
    def __init__(self, spec, name):
        super().__init__(spec, internal=False)
        self.name = name

    def stop(self):
        log(f"BREAKPOINT HIT: {self.name}")
        log(gdb.execute("bt 5", to_string=True))
        if self.name == "eventpoll_release_file_loop":
            epi = gdb.parse_and_eval("$x22") # Attempt to grab epi based on typical register mapping in AArch64 for this loop
            log(f"  epi (approx x22) = {epi}")
        return False

class SetupBP(gdb.Breakpoint):
    def __init__(self):
        super().__init__("__arm64_sys_epoll_ctl", internal=False)

    def stop(self):
        log("Setup Breakpoint Hit! Setting trace points...")
        TraceBP("ep_clear_and_put", "ep_clear_and_put")
        TraceBP("eventpoll_release_file", "eventpoll_release_file")
        TraceBP("mutex_lock_nested", "mutex_lock_nested")
        TraceBP("__ep_remove", "__ep_remove")
        self.enabled = False
        return False

SetupBP()
gdb.execute("continue")
gdb.execute("quit")
