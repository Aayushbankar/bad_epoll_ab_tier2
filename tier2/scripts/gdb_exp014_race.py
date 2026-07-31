import gdb
import time

def log(msg):
    print(f"[*] {msg}", flush=True)

log("Starting EXP-014: eventpoll_release_file UAF trace...")

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

# We will break in eventpoll_release_file at the mutex_lock_nested call!
# Disassembly of eventpoll_release_file:
# 0xffffffc0083bcd8c <+60>:	add	x19, x0, #0x28 // file->f_ep
# 0xffffffc0083bcda8 <+88>:	ldr	x22, [x19] // load first epi
# ...
# 0xffffffc0083bcdbc <+108>:	ldr	x0, [x22, #72] // ep = epi->ep
# 0xffffffc0083bcdc0 <+112>:	mov	w1, #0x0       // 0
# 0xffffffc0083bcdc4 <+116>:	bl	0xffffffc008f32240 <mutex_lock_nested>

class EpReleaseBP(gdb.Breakpoint):
    def __init__(self, spec):
        super().__init__(spec, internal=False)

    def stop(self):
        try:
            log("BREAKPOINT HIT: eventpoll_release_file loop")
            epi = gdb.parse_and_eval("$x22") # this register holds epi usually
            log(f"    epi = {epi}")
            # If we just pause here, Thread A will run and free this epi!
            log("Sleeping 2 seconds to let Thread A win the race and free the epi...")
            time.sleep(2)
            log("Resuming Thread B! Let's see what happens...")
        except Exception as e:
            log(f"Error in BP: {e}")
        return False

class EpRemoveSafeBP(gdb.Breakpoint):
    def __init__(self, spec):
        super().__init__(spec, internal=False)

    def stop(self):
        try:
            log("BREAKPOINT HIT: ep_remove_safe")
            epi = gdb.parse_and_eval("$x1")
            log(f"    ep_remove_safe called with epi = {epi}")
            bt = gdb.execute("bt 3", to_string=True)
            log(bt)
        except Exception as e:
            pass
        return False

class SetupBreakpoint(gdb.Breakpoint):
    def __init__(self, spec):
        super().__init__(spec, internal=False)

    def stop(self):
        log("Setup Breakpoint Hit! Setting internal breakpoints...")
        try:
            EpReleaseBP("eventpoll_release_file")
            EpRemoveSafeBP("ep_remove_safe")
        except Exception as e:
            log(f"Error setting breakpoints: {e}")
        self.enabled = False
        return False

SetupBreakpoint("__arm64_sys_epoll_ctl")

log("Continuing execution...")
gdb.execute("continue")

log("Done.")
gdb.execute("quit")
