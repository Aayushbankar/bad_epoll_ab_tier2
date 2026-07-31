import gdb
import time

def log(msg):
    print(f"[*] {msg}", flush=True)

log("Starting EXP-012 drain verification...")

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
            log(f"Connection failed: {e}, retrying...")
            time.sleep(1)
    return False

if not connect():
    log("Failed to connect.")
    gdb.execute("quit")

class DrainBreakpoint(gdb.Breakpoint):
    def __init__(self, spec, name):
        super().__init__(spec, internal=False)
        self.name = name

    def stop(self):
        log(f"BREAKPOINT HIT: {self.name}")
        # Print backtrace to understand where we are
        try:
            bt = gdb.execute("bt 3", to_string=True)
            log(f"BT:\\n{bt}")
        except:
            pass
        return False

class SetupBreakpoint(gdb.Breakpoint):
    def __init__(self, spec):
        super().__init__(spec, internal=False)

    def stop(self):
        log("Setup Breakpoint Hit! Harness is running. Setting internal breakpoints...")
        try:
            DrainBreakpoint("fs/eventpoll.c:806", "__ep_remove (entry)")
            DrainBreakpoint("fs/eventpoll.c:648", "remove_wait_queue (ep_unregister_pollwait)")
            DrainBreakpoint("fs/eventpoll.c:844", "list_del_init (corrupting write)")
        except Exception as e:
            log(f"Error setting breakpoints: {e}")
            
        self.enabled = False
        return False

log("Setting setup breakpoint on __arm64_sys_epoll_ctl...")
SetupBreakpoint("__arm64_sys_epoll_ctl")

log("Continuing execution...")
gdb.execute("continue")

log("Done.")
gdb.execute("quit")
