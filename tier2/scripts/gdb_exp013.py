import gdb
import time

def log(msg):
    print(f"[*] {msg}", flush=True)

print("=== EXP-013 GDB Trace ===", flush=True)

class EpEventpollReleaseBreakpoint(gdb.Breakpoint):
    def stop(self):
        try:
            # ep_eventpoll_release(struct inode *inode, struct file *file)
            file_ptr = gdb.parse_and_eval("$x1")
            ep = gdb.parse_and_eval(f"((struct file *){file_ptr})->private_data")
            log(f"ep_eventpoll_release called! file={file_ptr}, ep={ep}")
        except Exception as e:
            log(f"EpEventpollRelease exception: {e}")
        return False

class EpRemoveBreakpoint(gdb.Breakpoint):
    def stop(self):
        try:
            # __ep_remove(struct eventpoll *ep, struct epitem *epi, bool force_clear)
            ep = gdb.parse_and_eval("$x0")
            epi = gdb.parse_and_eval("$x1")
            file_ptr = gdb.parse_and_eval(f"((struct epitem *){epi})->ffd.file")
            inner_ep = gdb.parse_and_eval(f"((struct file *){file_ptr})->private_data")
            log(f"__ep_remove called! ep={ep}, epi={epi}")
            log(f"inner_epoll file={file_ptr}, inner_ep={inner_ep}")
            
            offset = 160
            val_before = gdb.parse_and_eval(f"*(unsigned long *)({inner_ep} + {offset})")
            log(f"Memory at inner_ep+160 BEFORE hlist_del_rcu: {val_before}")
            
            gdb.execute(f"watch *(unsigned long *)({inner_ep} + {offset})")
            gdb.execute(f"commands\nsilent\nprintf \"\\n[!!!] WATCHPOINT HIT: inner_ep+160 changed!\\n\"\ncontinue\nend")
            
        except Exception as e:
            log(f"EpRemove exception: {e}")
        return False

gdb.execute("target remote :12345")
log("--- SETTING BREAKPOINTS ---")
EpEventpollReleaseBreakpoint("ep_eventpoll_release", type=gdb.BP_HARDWARE_BREAKPOINT)
EpRemoveBreakpoint("__ep_remove", type=gdb.BP_HARDWARE_BREAKPOINT)
log("--- BREAKPOINTS SET ---")

def handle_exit(event):
    log("Process exited. Quitting...")
    gdb.execute("quit")

gdb.events.exited.connect(handle_exit)

try:
    gdb.execute("continue")
except gdb.error as e:
    log(f"Execution finished or connection dropped: {e}")
gdb.execute("quit")
