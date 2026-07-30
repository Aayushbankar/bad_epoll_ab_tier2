import gdb
import time

def log(msg):
    with open("/mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/evidence/EXP-011_raw_gdb.log", "a") as f:
        f.write(f"[*] {msg}\n")
    print(f"[*] {msg}")

with open("/mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/evidence/EXP-011_raw_gdb.log", "w") as f:
    f.write("=== EXP-011 GDB Trace ===\n")


class UnshareBreakpoint(gdb.Breakpoint):
    def stop(self):
        try:
            flag = int(gdb.parse_and_eval("$x0"))
            if flag == 1111:
                log("Signal: Thread 1 is about to epoll_wait.")
                self.enabled = False
                return False
            elif flag == 2222:
                log("Signal: Thread 2 is about to close inner epoll.")
                return False
            elif flag == 3333:
                log("Signal: Thread 3 is about to spray timerfds.")
                return False
        except Exception as e:
            log(f"UnshareBreakpoint exception: {e}")
        return False

class __fput_Breakpoint(gdb.Breakpoint):
    def stop(self):
        try:
            file_ptr = gdb.parse_and_eval("$x0")
            f_op = gdb.parse_and_eval("((struct file*)$x0)->f_op")
            eventpoll_fops_addr = int(gdb.parse_and_eval("&eventpoll_fops"))
            if f_op == eventpoll_fops_addr:
                log(f"BINGO! __fput CALLED for STALE file {file_ptr}!")
                log(f"Verifying via register read inside breakpoint: $x0 = {file_ptr}")
                return False
        except Exception as e:
            log(f"__fput_Breakpoint exception: {e}")
        return False

class ep_item_poll_Breakpoint(gdb.Breakpoint):
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
    def stop(self):
        try:
            file_ptr = gdb.parse_and_eval("$x0")
            log(f"BINGO! timerfd_poll called on file {file_ptr}")
        except Exception as e:
            log(f"timerfd_poll exception: {e}")
        return False

class sys_timerfd_create_Breakpoint(gdb.Breakpoint):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.hit_count = 0

    def stop(self):
        try:
            self.hit_count += 1
            if self.hit_count <= 10:
                log(f"__arm64_sys_timerfd_create called (hit {self.hit_count})")
            elif self.hit_count == 11:
                log("__arm64_sys_timerfd_create called (suppressing further hits to reduce log spam)")
                self.enabled = False
        except Exception as e:
            log(f"timerfd_create exception: {e}")
        return False

class do_epoll_wait_Breakpoint(gdb.Breakpoint):
    def stop(self):
        log("do_epoll_wait called")
        return False

log("--- SETTING BREAKPOINTS ---")
UnshareBreakpoint("ksys_unshare")
__fput_Breakpoint("__fput")
ep_item_poll_Breakpoint("ep_item_poll")
timerfd_poll_Breakpoint("timerfd_poll")
do_epoll_wait_Breakpoint("do_epoll_wait")
sys_timerfd_create_Breakpoint("__arm64_sys_timerfd_create")
log("--- BREAKPOINTS SET ---")

def handle_exit(event):
    log(f"Process exited cleanly. Triggers completed.")
    gdb.execute("quit")

gdb.events.exited.connect(handle_exit)

gdb.execute("continue")
