import gdb
import time

def log(msg):
    print(f"[*] {msg}")
    with open("/mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/evidence/EXP-011_raw_gdb.log", "a") as f:
        f.write(f"[*] {msg}\n")

with open("/mnt/work/company/cyphermatrix/repos/bad-epoll-lab/tier2/evidence/EXP-011_raw_gdb.log", "w") as f:
    f.write("=== EXP-011 GDB Trace ===\n")

class UnshareBreakpoint(gdb.Breakpoint):
    def stop(self):
        flag = int(gdb.parse_and_eval("$x0"))
        
        if flag == 1111:
            log("Signal: Thread 1 is about to epoll_wait. Enabling ep_item_poll breakpoint.")
            gdb.execute("b *ep_item_poll")
            # Also break at timerfd_poll to see if it's called
            gdb.execute("b *timerfd_poll")
            return False
            
        elif flag == 2222:
            log("Signal: Thread 2 is about to close inner epoll.")
            # Break at __fput to observe the file free
            gdb.execute("b __fput")
            return False
            
        elif flag == 3333:
            log("Signal: Thread 3 is about to spray timerfds.")
            return False
            
        return False

class __fput_Breakpoint(gdb.Breakpoint):
    def stop(self):
        file_ptr = gdb.parse_and_eval("$x0")
        f_op = gdb.parse_and_eval("((struct file*)$x0)->f_op")
        
        # Check if this is our eventpoll file
        eventpoll_fops_addr = int(gdb.parse_and_eval("&eventpoll_fops"))
        if f_op == eventpoll_fops_addr:
            log(f"BINGO! __fput CALLED for STALE file {file_ptr}!")
            log(f"Verifying via register read inside breakpoint: $x0 = {file_ptr}")
            return False
        return False

class ep_item_poll_Breakpoint(gdb.Breakpoint):
    def stop(self):
        epi = gdb.parse_and_eval("$x0")
        file_ptr = gdb.parse_and_eval("((struct epitem*)$x0)->ffd.file")
        f_op = gdb.parse_and_eval("((struct file*)((struct epitem*)$x0)->ffd.file)->f_op")
        log(f"BINGO! ep_item_poll called on epi={epi}")
        log(f"epi->ffd.file = {file_ptr}")
        log(f"file->f_op = {f_op}")
        return False

class timerfd_poll_Breakpoint(gdb.Breakpoint):
    def stop(self):
        file_ptr = gdb.parse_and_eval("$x0")
        log(f"BINGO! timerfd_poll called on file {file_ptr}")
        return False

gdb.execute("b ksys_unshare")
UnshareBreakpoint("ksys_unshare")
__fput_Breakpoint("__fput")
ep_item_poll_Breakpoint("ep_item_poll")
timerfd_poll_Breakpoint("timerfd_poll")

gdb.execute("continue")
