import gdb

def log(msg):
    with open("/tmp/gdb_trace.log", "a") as f:
        f.write(msg + "\n")

class RaceControl(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*(__ep_remove + 0x19c)")
        self.target_epoll = 0

    def stop(self):
        log(f"[*] Thread A hit 0xffff8000802bc9c0 (WRITE_ONCE finished).")
        
        target_file = int(gdb.parse_and_eval("$x23"))
        self.target_epoll = int(gdb.parse_and_eval(f"*(long*)({target_file} + 32)"))
        
        log(f"[*] target_file={hex(target_file)}, target_epoll={hex(self.target_epoll)}")
        
        self.orig_insn = int(gdb.parse_and_eval(f"*(int*)($pc)"))
        log(f"[*] Original instruction at PC: {hex(self.orig_insn)}")
        
        gdb.execute(f"set *(int*)($pc) = 0x14000000")
        log("[*] Patched PC with infinite loop to suspend Thread A!")
        
        EpFreeBreak(self.target_epoll)
        # Also break on eventpoll_release_file to see if Thread B hits it
        EventpollReleaseBreak()
        FputBreak()
        self.enabled = False
        return False

class FputBreak(gdb.Breakpoint):
    def __init__(self):
        super().__init__("__fput")

    def stop(self):
        file_ptr = int(gdb.parse_and_eval("$x0"))
        log(f"[*] Thread hit __fput({hex(file_ptr)})!")
        return False

class EventpollReleaseBreak(gdb.Breakpoint):
    def __init__(self):
        super().__init__("eventpoll_release_file")

    def stop(self):
        file_ptr = int(gdb.parse_and_eval("$x0"))
        log(f"[*] Thread B hit eventpoll_release_file({hex(file_ptr)})!")
        return False

class EpFreeBreak(gdb.Breakpoint):
    def __init__(self, target_epoll):
        super().__init__("*(__ep_clear_and_put + 292)" if False else "*0xffff8000802bcbb4")
        self.target_epoll = target_epoll

    def stop(self):
        ep = int(gdb.parse_and_eval("$x22"))
        if ep == self.target_epoll:
            log(f"[*] Thread B hit ep_free({hex(ep)})!")
            EpFreeFinish(self.target_epoll)
            self.enabled = False
        return False

class EpFreeFinish(gdb.Breakpoint):
    def __init__(self, target_epoll):
        # Break exactly on the instruction AFTER kfree in ep_clear_and_put
        super().__init__("*0xffff8000802bcbbc")
        self.target_epoll = target_epoll

    def stop(self):
        log(f"[*] Thread B finished ep_free. target_epoll is now FREED!")
        
        watch_addr = self.target_epoll + 160
        gdb.execute(f"watch *(long*)({watch_addr})")
        log(f"[*] Set HW watchpoint on {hex(watch_addr)}")
        
        pc_addr = int(gdb.parse_and_eval("__ep_remove + 0x19c"))
        gdb.execute(f"set *(int*)({pc_addr}) = 0x91006000")
        log(f"[*] Restored Thread A's instruction at {hex(pc_addr)}!")
        
        return False

RaceControl()
log("Breakpoints set. Continuing execution.")
gdb.execute("c")
