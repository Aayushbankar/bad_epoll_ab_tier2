# exp_hyp011_p0_gdb.py — HYP-011 Part 0: Reclaim Miss Root-Cause Discriminator
import gdb
import time
import os

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

os.makedirs("evidence/HYP-011", exist_ok=True)
gdb.execute("set logging file evidence/HYP-011/HYP-011_part0_raw_gdb.log")
gdb.execute("set logging overwrite on")
gdb.execute("set logging enabled on")

def log(msg):
    print(f"[HYP-011-GDB] {msg}", flush=True)

def to_u64(val):
    return int(val) & 0xFFFFFFFFFFFFFFFF

def to_i64(val):
    v = int(val) & 0xFFFFFFFFFFFFFFFF
    if v >= 0x8000000000000000:
        return v - 0x10000000000000000
    return v

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

g_target_inner_file = 0
PAGE_OFFSET = 0xffffff8000000000
VMEMMAP_START = 0xfffffffe00000000

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
            log(f"[HYP-011] Thread B entered __fput(file={hex(file_ptr)})")
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

class PrctlBreakpoint(gdb.Breakpoint):
    def __init__(self):
        super(PrctlBreakpoint, self).__init__("__arm64_sys_prctl", internal=False)

    def stop(self):
        global g_target_inner_file
        try:
            regs = to_u64(gdb.parse_and_eval("$x0"))
            option = int(gdb.parse_and_eval(f"*(unsigned long*)({regs})"))
        except Exception as e:
            return False

        if option == 0x1337:
            log("=================================================================")
            log("[HYP-011 PART 0 DISCRIMINATOR] marker prctl(0x1337) intercepted!")
            log(f"  Target victim file: {hex(g_target_inner_file)}")

            if g_target_inner_file == 0:
                log("  [!] Error: g_target_inner_file is 0")
                return False

            page_base = g_target_inner_file & ~0xFFF
            pfn_idx = (g_target_inner_file - PAGE_OFFSET) >> 12
            page_addr = VMEMMAP_START + (pfn_idx * 64)

            log(f"  Memory Geometry Derivation:")
            log(f"    PAGE_OFFSET    = {hex(PAGE_OFFSET)}")
            log(f"    VMEMMAP_START  = {hex(VMEMMAP_START)}")
            log(f"    Victim File    = {hex(g_target_inner_file)}")
            log(f"    Slab Page Base = {hex(page_base)}")
            log(f"    PFN Index      = {pfn_idx} ({hex(pfn_idx)})")
            log(f"    struct slab *  = {hex(page_addr)}")

            try:
                page_flags = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({page_addr} + 0)"))
                slab_cache = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({page_addr} + 24)"))
                freelist = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({page_addr} + 32)"))
                counters = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({page_addr} + 40)"))
                inuse = counters & 0xFFFF
                objects = (counters >> 16) & 0x7FFF
                frozen = (counters >> 31) & 1
                refcount = int(gdb.parse_and_eval(f"*(int*)({page_addr} + 52)"))
                memcg_data = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({page_addr} + 56)"))

                log(f"  struct slab/page dump at {hex(page_addr)}:")
                log(f"    flags:      {hex(page_flags)}")
                log(f"    slab_cache: {hex(slab_cache)}")
                log(f"    freelist:   {hex(freelist)}")
                log(f"    counters:   {hex(counters)} (inuse={inuse}, objects={objects}, frozen={frozen})")
                log(f"    _refcount:  {refcount}")
                log(f"    memcg_data: {hex(memcg_data)}")

                if slab_cache != 0:
                    log(f"  [DISCRIMINATOR RESULT] slab_cache is NON-NULL ({hex(slab_cache)})!")
                    log("  --> Slab page is STILL OWNED BY SLUB (partial/pinned slab).")
                    log("  --> Walking all 16 slots of slab page to identify live neighbors...")

                    for slot_idx in range(16):
                        slot_addr = page_base + (slot_idx * 256)
                        try:
                            f_cnt = to_i64(gdb.parse_and_eval(f"*(long*)({slot_addr} + 56)"))
                            f_ver = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({slot_addr} + 184)"))
                            f_ino = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({slot_addr} + 32)"))
                            f_dentry = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({slot_addr} + 24)"))
                            
                            name_str = "<none>"
                            if f_dentry != 0:
                                try:
                                    name_bytes = gdb.selected_inferior().read_memory(f_dentry + 56, 32)
                                    name_str = name_bytes.tobytes().split(b'\x00')[0].decode('latin1', errors='ignore')
                                    if not name_str:
                                        dname_ptr = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({f_dentry} + 40)"))
                                        if dname_ptr != 0:
                                            name_b = gdb.selected_inferior().read_memory(dname_ptr, 32)
                                            name_str = name_b.tobytes().split(b'\x00')[0].decode('latin1', errors='ignore')
                                except Exception as e:
                                    name_str = f"<err:{e}>"

                            is_victim = (slot_addr == g_target_inner_file)
                            status = "FREED_CANARY" if f_ver == 0xdeadf11e else ("VICTIM" if is_victim else ("LIVE" if f_cnt > 0 else "ZERO_COUNT"))
                            log(f"    Slot [{slot_idx:02d}] {hex(slot_addr)}: f_count={f_cnt:2d} f_ver={hex(f_ver)} dentry={name_str:16s} [{status}]")
                        except Exception as e:
                            log(f"    Slot [{slot_idx:02d}] {hex(slot_addr)}: read error: {e}")

                else:
                    log(f"  [DISCRIMINATOR RESULT] slab_cache is NULL ({hex(slab_cache)})!")
                    log("  --> Page was FULLY FREED back to the buddy allocator!")
                    log("  --> Hypothesis H-c (migratetype segregation / buddy free-list behavior) STANDS.")

            except Exception as e:
                log(f"  [!] Error reading page struct: {e}")

            log("=================================================================")
            return False

        return False

# Register breakpoints
FputBreakpoint()
PrctlBreakpoint()

log("Breakpoints installed. Continuing execution...")
gdb.execute("continue")

log("Execution completed or breakpoint hit.")
gdb.execute("quit")
