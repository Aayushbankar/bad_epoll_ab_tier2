import gdb
import time

def log(msg):
    print(f"[*] {msg}", flush=True)

log("Starting EXP-012 drain verification with identity tracking...")

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

tracked_epi = None

class RemoveWaitQueueBreakpoint(gdb.Breakpoint):
    def __init__(self, spec):
        super().__init__(spec, internal=False)

    def stop(self):
        global tracked_epi
        try:
            # remove_wait_queue(wq_head, wq_entry)
            # x0 = whead, x1 = wq_entry (&pwq->wait)
            # pwq->wait is at offset 16 in struct eppoll_entry
            wq_entry = gdb.parse_and_eval("$x1")
            pwq = int(wq_entry) - 16
            
            # epi is at pwq->base (offset 8)
            epi = gdb.parse_and_eval(f"*(struct epitem **){pwq + 8}")
            tracked_epi = epi
            
            log(f"BREAKPOINT HIT: remove_wait_queue")
            log(f"    wq_entry = {hex(int(wq_entry))}")
            log(f"    pwq (wq_entry - 16) = {hex(pwq)}")
            log(f"    pwq->base (epi) = {epi}")
        except Exception as e:
            log(f"remove_wait_queue error: {e}")
        return False

class EpRemoveBreakpoint(gdb.Breakpoint):
    def __init__(self, spec):
        super().__init__(spec, internal=False)

    def stop(self):
        global tracked_epi
        try:
            # __ep_remove(struct eventpoll *ep, struct epitem *epi, bool force)
            # epi is in $x1
            epi = gdb.parse_and_eval("$x1")
            log(f"BREAKPOINT HIT: __ep_remove (entry)")
            log(f"    epi = {epi}")
            if tracked_epi is not None:
                if str(epi) == str(tracked_epi):
                    log(f"    IDENTITY MATCH: The epi being removed is the EXACT SAME one drained in remove_wait_queue!")
                else:
                    log(f"    IDENTITY MISMATCH: expected {tracked_epi}, got {epi}")
        except Exception as e:
            log(f"__ep_remove error: {e}")
        return False

class SetupBreakpoint(gdb.Breakpoint):
    def __init__(self, spec):
        super().__init__(spec, internal=False)

    def stop(self):
        log("Setup Breakpoint Hit! Harness is running. Setting internal breakpoints...")
        try:
            EpRemoveBreakpoint("fs/eventpoll.c:806")
            RemoveWaitQueueBreakpoint("remove_wait_queue")
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
