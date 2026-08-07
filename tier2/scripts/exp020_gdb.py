import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

gdb.execute("set logging file evidence/EXP-020_raw_gdb.log")
gdb.execute("set logging overwrite on")
gdb.execute("set logging enabled on")

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

class RaceControl(gdb.Breakpoint):
    def __init__(self):
        super(RaceControl, self).__init__("*(__ep_remove + 0x19c)", internal=False)
        self.target_epoll = None

    def stop(self):
        log(f"Thread A hit __ep_remove+0x19c (WRITE_ONCE finished).")
        
        target_file = int(gdb.parse_and_eval("$x23"))
        self.target_epoll = int(gdb.parse_and_eval(f"*(long*)({target_file} + 32)"))
        
        log(f"target_file={hex(target_file)}, target_epoll={hex(self.target_epoll)}")
        
        self.orig_insn = int(gdb.parse_and_eval(f"*(int*)($pc)"))
        self.pc = int(gdb.parse_and_eval(f"$pc"))
        log(f"Original instruction at PC: {hex(self.orig_insn)}")
        
        gdb.execute(f"set *(int*)($pc) = 0x14000000")
        log("Patched PC with infinite loop to suspend Thread A!")
        
        EpFreeBreak(self)
        self.enabled = False
        return False

class EpFreeBreak(gdb.Breakpoint):
    def __init__(self, race_control):
        super(EpFreeBreak, self).__init__("*0xffff8000802bcbb4", internal=False)
        self.race_control = race_control

    def stop(self):
        ep = int(gdb.parse_and_eval("$x22"))
        log(f"Thread B hit ep_free({hex(ep)})!")
        EpFreeFinish(ep, self.race_control)
        self.enabled = False
        return False

import threading
import os
import signal
import time

class EpFreeFinish(gdb.Breakpoint):
    def __init__(self, target_epoll, race_control):
        super().__init__("*0xffff8000802bcbbc")
        self.target_epoll = target_epoll
        self.race_control = race_control

    def stop(self):
        log(f"Thread B finished ep_free. target_epoll is now FREED!")
        
        def interrupt_later():
            time.sleep(2.0)
            log("Timer fired! Interrupting GDB to check spray and restore PC...")
            os.kill(os.getpid(), signal.SIGINT)
            
        t = threading.Thread(target=interrupt_later)
        t.daemon = True
        t.start()
        
        self.enabled = False
        return False

def stop_handler(event):
    if isinstance(event, gdb.SignalEvent):
        if event.stop_signal == "SIGINT":
            log("Interrupted! Restoring PC and setting watchpoint...")
            try:
                addr = RaceControl_instance.target_epoll
                mem_dump = gdb.execute(f"x/24gx {addr}", to_string=True)
                log("\n" + mem_dump)
                
                # Restore original instruction
                gdb.execute(f"set *(int*)({RaceControl_instance.pc}) = {RaceControl_instance.orig_insn}")
                log("Restored original instruction.")
                
                log("Setting breakpoint at rb_erase_cached (rb_erase in this kernel)")
                # In kernel it's rb_erase. __ep_remove calls it around offset 248. Let's break there or at rb_erase directly.
                # Actually, EXP-020 requires instruction-level trace of rb_erase_cached.
                # Let's break at __ep_remove+0xf8 (where rb_erase is called) or at rb_erase.
                gdb.execute(f"b *0xffff8000802bc91c") # Call to rb_erase in __ep_remove
                
                # We will continue until the call to rb_erase
                log("Executing continue to reach rb_erase...")
                
                try:
                    gdb.execute("continue")
                except Exception as e:
                    log(f"Continue interrupted: {e}")
                
                log("At call to rb_erase! Starting stepi trace...")
                # We want to record instructions and registers
                for i in range(25):
                    log(f"--- Step {i} ---")
                    try:
                        insn = gdb.execute("x/i $pc", to_string=True).strip()
                        log(f"INSN: {insn}")
                        
                        # Print relevant registers if needed
                        # Or just dump x0-x5 to see pointer dereferences
                        regs = gdb.execute("info registers x0 x1 x2 x3 x4 x5", to_string=True)
                        log(regs)
                        
                        gdb.execute("stepi")
                    except Exception as e:
                        log(f"Stepi error: {e}")
                        break
                        
            except Exception as e:
                log(f"Error in stop handler: {e}")
                
            log("Finished. Exiting GDB.")
            gdb.execute("quit")

gdb.events.stop.connect(stop_handler)

RaceControl_instance = RaceControl()
log("Breakpoints set. Continuing execution.")

# Start a timer to interrupt GDB if it hangs for too long
def interrupt_if_hung():
    time.sleep(30.0)
    log("Hang timeout! Quitting...")
    gdb.execute("quit")
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

