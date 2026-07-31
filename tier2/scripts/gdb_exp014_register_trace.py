import gdb

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
            import time
            time.sleep(1)
    return False

if not connect():
    gdb.execute("quit")

# We want to trace eventpoll_release_file:
# ffff8000802bdbac: cbz x1, ffff8000802bdc20 <eventpoll_release_file+0xb4> (after getting epi)
# ffff8000802bdbb8: ldr x19, [x20, #72] (ep = epi->ep)
# ffff8000802bdbc0: bl ffff800080ccac3c <mutex_lock> (block point)
# ffff8000802bdbc4: mov x1, x20 (wake point, preparing for __ep_remove)
# ffff8000802bdbd0: bl ffff8000802bc824 <__ep_remove>

class TraceEPRelease(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*0xffff8000802bdbbc", internal=False)

    def stop(self):
        log("Captured epi (before ep=epi->ep read):")
        gdb.execute("info registers x20")
        return False

class TraceEPRead(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*0xffff8000802bdbc0", internal=False)

    def stop(self):
        log("Read ep=epi->ep (before mutex_lock):")
        gdb.execute("info registers x19")
        return False

class TraceWake(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*0xffff8000802bdbc4", internal=False)

    def stop(self):
        log("Woke from mutex_lock (before __ep_remove):")
        gdb.execute("info registers x19 x20")
        return False

class TraceEPRemove(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*0xffff8000802bdbd0", internal=False)

    def stop(self):
        log("Calling __ep_remove with arguments:")
        gdb.execute("info registers x0 x1 x2")
        return False

class SetupBP(gdb.Breakpoint):
    def __init__(self):
        super().__init__("__arm64_sys_epoll_ctl", internal=False)

    def stop(self):
        log("Setup Breakpoint Hit! Setting trace points...")
        TraceEPRelease()
        TraceEPRead()
        TraceWake()
        TraceEPRemove()
        self.enabled = False
        return False

SetupBP()
gdb.execute("continue")
gdb.execute("quit")
