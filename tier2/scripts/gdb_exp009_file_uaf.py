import gdb
import time

stale_file_ptr = 0
orig_insn = 0
pc_addr = 0

bp_ep_remove = None
bp_file_free = None
bp_fput = None
bp_fput_match = None
bp_ep_clear = None
bp_mutex_lock = None

def to_u64(val):
    return val & 0xffffffffffffffff

class EpClearBreakpoint(gdb.Breakpoint):
    def stop(self):
        print("\\n[*] Thread 2: ep_clear_and_put CALLED!")
        return False

class MutexLockBreakpoint(gdb.Breakpoint):
    def stop(self):
        try:
            lock = to_u64(int(gdb.parse_and_eval("$x0")))
            print(f"\\n[*] Thread 2: mutex_lock CALLED for 0x{lock:x}!")
        except Exception as e:
            pass
        return False

class FputMatchBreakpoint(gdb.Breakpoint):
    def stop(self):
        global stale_file_ptr, bp_ep_clear, bp_mutex_lock
        try:
            f = to_u64(int(gdb.parse_and_eval("$x0")))
            if f == stale_file_ptr and stale_file_ptr != 0:
                print(f"\\n[*] BINGO! __fput CALLED for STALE file 0x{f:x}!")
                bp_ep_clear = EpClearBreakpoint("ep_clear_and_put")
                bp_mutex_lock = MutexLockBreakpoint("mutex_lock")
        except Exception as e:
            pass
        return False

class Thread2Breakpoint(gdb.Breakpoint):
    def stop(self):
        global stale_file_ptr
        try:
            f = to_u64(int(gdb.parse_and_eval("$x0")))
            if f == stale_file_ptr and stale_file_ptr != 0:
                print(f"\\n[*] Thread 2: STALE file_free MATCHES! 0x{f:x}")
                self.enabled = False
        except Exception as e:
            pass
        return False

class Thread1Breakpoint(gdb.Breakpoint):
    def stop(self):
        global stale_file_ptr, orig_insn, pc_addr, bp_fput_match
        try:
            epi = to_u64(int(gdb.parse_and_eval("$x1")))
            stale_file_ptr = to_u64(int(gdb.parse_and_eval(f"*(long*)({epi} + 0x30)")))
            
            f_op = to_u64(int(gdb.parse_and_eval(f"*(long*)({stale_file_ptr} + 0x28)")))
            f_count = int(gdb.parse_and_eval(f"*(long*)({stale_file_ptr} + 0x38)"))
            
            print("\\n=======================================================")
            print("=== STALE FILE POINTER CAPTURED IN THREAD 1 ===")
            print(f"    file_ptr: 0x{stale_file_ptr:x}")
            print(f"    f_op:     0x{f_op:x}")
            print(f"    f_count:  {f_count}")
            print("=======================================================\\n")
            
            bp_fput_match = FputMatchBreakpoint("__fput")
            
            func_addr = int(gdb.parse_and_eval("&__ep_remove"))
            pc_addr = func_addr + 280
            
            val = gdb.execute(f"x/1wx {pc_addr}", to_string=True)
            orig_insn_str = val.split(":")[1].strip()
            orig_insn = int(orig_insn_str, 16)
            
            print(f"[*] Patching instruction at 0x{pc_addr:x} to an infinite loop (B .)")
            gdb.execute(f"set {{int}}{pc_addr} = 0x14000000")
            
        except Exception as e:
            print(f"Exception in Thread1Breakpoint: {e}")
            
        self.enabled = False
        return False

class SignalBreakpoint(gdb.Breakpoint):
    def stop(self):
        global bp_ep_remove, bp_file_free, stale_file_ptr
        try:
            flags = int(gdb.parse_and_eval("$x0"))
            
            if flags == 1111:
                print("\\n[*] Signal: Thread 1 is about to close outer epoll. Enabling __ep_remove breakpoint.")
                bp_ep_remove = Thread1Breakpoint("__ep_remove")
                gdb.execute("return -22")
                return False
            elif flags == 2222:
                print("\\n[*] Signal: Thread 2 is about to close inner epoll.")
                bp_file_free = Thread2Breakpoint("file_free")
                gdb.execute("return -22")
                return False
        except Exception as e:
            pass
        return False

def setup():
    gdb.execute("set pagination off")
    gdb.execute("set non-stop off")
    
    retries = 10
    while retries > 0:
        try:
            gdb.execute("target remote :1234")
            break
        except gdb.error:
            time.sleep(1)
            retries -= 1

    SignalBreakpoint("ksys_unshare")
    gdb.execute("continue")

setup()
