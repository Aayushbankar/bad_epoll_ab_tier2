import gdb
import time
import os

mode = os.environ.get("TEST_MODE", "A")

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

def log(msg):
    print(f"[M4-GDB] {msg}", flush=True)

def to_u64(val):
    return int(val) & 0xFFFFFFFFFFFFFFFF

for i in range(10):
    try:
        gdb.execute("target remote :1234")
        log("Connected to QEMU on :1234")
        break
    except gdb.error:
        time.sleep(1)

g_epB_file = 0

class FputBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(FputBreakpoint, self).__init__("__fput", internal=False)

    def stop(self):
        global g_epB_file
        file_ptr = to_u64(gdb.parse_and_eval("$x0"))
        
        if g_epB_file == 0:
            try:
                f_ep = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + 208)"))
            except Exception:
                return False

            if f_ep != 0:
                log(f"Intercepted __fput for epB (file={hex(file_ptr)}). Saving address.")
                g_epB_file = file_ptr
                gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")
                self.enabled = False
        return False

def inject_layout():
    if mode == 'A':
        pass
    elif mode == 'B':
        empty_zero_page = 0xffffffc0098fd000
        comm_inode = 0xffffffc00973e258 - 0x40
        gdb.execute(f"set *(unsigned long*)({g_epB_file} + 32) = {comm_inode}")
        gdb.execute(f"set *(unsigned long*)({g_epB_file} + 40) = {empty_zero_page}")
        gdb.execute(f"set *(unsigned int*)({g_epB_file} + 48) = 0")
        gdb.execute(f"set *(unsigned long*)({g_epB_file} + 56) = 1")
        gdb.execute(f"set *(unsigned int*)({g_epB_file} + 68) = 0x20000")
        gdb.execute(f"set *(unsigned long*)({g_epB_file} + 208) = {empty_zero_page}")
        gdb.execute(f"set *(unsigned long*)({g_epB_file} + 184) = 0")

class EpRemoveBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(EpRemoveBreakpoint, self).__init__("__ep_remove", internal=False)

    def stop(self):
        global g_epB_file
        if g_epB_file != 0:
            epi_ptr = to_u64(gdb.parse_and_eval("$x1"))
            try:
                epi_file = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({epi_ptr} + 48)"))
            except:
                return False
            if epi_file == g_epB_file:
                log(f"Intercepted __ep_remove. Injecting layout for Mode {mode}")
                inject_layout()
                self.enabled = False
        return False

class EpShowFdinfoBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(EpShowFdinfoBreakpoint, self).__init__("ep_show_fdinfo", internal=False)

    def stop(self):
        global g_epB_file
        if g_epB_file != 0:
            log(f"Intercepted ep_show_fdinfo. Injecting layout for Mode {mode}")
            inject_layout()
        return False

bp_fput = FputBreakpoint()
bp_remove = EpRemoveBreakpoint()
bp_fdinfo = EpShowFdinfoBreakpoint()

log("Breakpoints installed. Continuing...")
gdb.execute("continue")
gdb.execute("quit")
