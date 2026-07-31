import gdb
import time

def log(msg):
    print(f"[*] {msg}", flush=True)

print("=== EXP-011 GDB Trace ===", flush=True)

while True:
    try:
        gdb.execute("target remote :1234")
        break
    except gdb.error:
        time.sleep(0.5)

class ep_item_poll_Breakpoint(gdb.Breakpoint):
    def __init__(self, *args, **kwargs):
        super().__init__(type=gdb.BP_HARDWARE_BREAKPOINT, *args, **kwargs)
    def stop(self):
        try:
            epi = gdb.parse_and_eval("$x0")
            file_ptr = gdb.parse_and_eval("((struct epitem*)$x0)->ffd.file")
            f_op = gdb.parse_and_eval("((struct file*)((struct epitem*)$x0)->ffd.file)->f_op")
            log(f"BINGO! ep_item_poll called on epi={epi}")
            log(f"epi->ffd.file = {file_ptr}")
            log(f"file->f_op = {f_op}")
        except Exception as e:
            log(f"ep_item_poll exception: {e}")
        return False

class timerfd_poll_Breakpoint(gdb.Breakpoint):
    def __init__(self, *args, **kwargs):
        super().__init__(type=gdb.BP_HARDWARE_BREAKPOINT, *args, **kwargs)
    def stop(self):
        try:
            file_ptr = gdb.parse_and_eval("$x0")
            log(f"BINGO! timerfd_poll called on file {file_ptr}")
        except Exception as e:
            log(f"timerfd_poll exception: {e}")
        return False

class sys_timerfd_create_Breakpoint(gdb.Breakpoint):
    def __init__(self, *args, **kwargs):
        super().__init__(type=gdb.BP_HARDWARE_BREAKPOINT, *args, **kwargs)
        self.my_hit_count = 0
    def stop(self):
        try:
            self.my_hit_count += 1
            if self.my_hit_count <= 10:
                log(f"__arm64_sys_timerfd_create called (hit {self.my_hit_count})")
            elif self.my_hit_count == 11:
                log("__arm64_sys_timerfd_create called (suppressing further hits to reduce log spam)")
                self.enabled = False
        except Exception as e:
            log(f"timerfd_create exception: {e}")
        return False

class do_epoll_wait_Breakpoint(gdb.Breakpoint):
    def __init__(self, *args, **kwargs):
        super().__init__(type=gdb.BP_HARDWARE_BREAKPOINT, *args, **kwargs)
    def stop(self):
        log("do_epoll_wait called")
        return False

log("--- SETTING BREAKPOINTS ---")
ep_item_poll_Breakpoint("ep_item_poll")
timerfd_poll_Breakpoint("timerfd_poll")
do_epoll_wait_Breakpoint("__arm64_sys_epoll_pwait")
sys_timerfd_create_Breakpoint("__arm64_sys_timerfd_create")
log("--- BREAKPOINTS SET ---")

def handle_exit(event):
    log(f"Process exited cleanly. Triggers completed.")
    gdb.execute("quit")

gdb.events.exited.connect(handle_exit)

gdb.execute("continue")
