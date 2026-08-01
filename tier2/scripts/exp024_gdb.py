"""
EXP-024: Clean re-test of dual-watch KASLR leak claim (VER-029/VER-030)

FIXED: Use correct symbol name 'eventpoll_fops' (not 'ep_fops').
Use GDB struct inspection for reliable offset calculation.
"""

import gdb
import time
import os
import signal
import threading

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set non-stop off")

LOG_FILE = "evidence/EXP-024_raw_gdb.log"
gdb.execute(f"set logging file {LOG_FILE}")
gdb.execute("set logging overwrite on")
gdb.execute("set logging on")

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

# Resolve key offsets using GDB's type knowledge
log("Resolving struct offsets...")

# Each offset resolved independently so one failure doesn't break others
f_op_offset = 16  # fallback
try:
    f_op_offset = int(gdb.parse_and_eval("(unsigned long)&((struct file*)0)->f_op"))
except:
    pass
log(f"  struct file->f_op offset: {f_op_offset}")

private_data_offset = 32  # fallback
try:
    private_data_offset = int(gdb.parse_and_eval("(unsigned long)&((struct file*)0)->private_data"))
except:
    pass
log(f"  struct file->private_data offset: {private_data_offset}")

f_ep_offset = 144  # fallback
try:
    f_ep_offset = int(gdb.parse_and_eval("(unsigned long)&((struct file*)0)->f_ep"))
except:
    pass
log(f"  struct file->f_ep offset: {f_ep_offset}")

eventpoll_fops_addr = 0
try:
    eventpoll_fops_addr = int(gdb.parse_and_eval("(unsigned long)&eventpoll_fops"))
except:
    pass
log(f"  eventpoll_fops: {hex(eventpoll_fops_addr)}")

refs_offset = 160  # confirmed by VER-020
try:
    refs_offset = int(gdb.parse_and_eval("(unsigned long)&((struct eventpoll*)0)->refs"))
except:
    pass
log(f"  struct eventpoll->refs offset: {refs_offset}")

# epi->ffd.file is at offset 48 from disassembly (ldr x24, [x1, #48])
ffd_file_offset = 48
log(f"  epi->ffd.file offset: {ffd_file_offset} (from disassembly)")

# Global state
inner_epoll_addr = 0
epi_addr = 0
write_captured = False
ep_free_inner_seen = False
refs_first_history = []
thread_a_trapped = False

class EpRemoveEntry(gdb.Breakpoint):
    """Break at __ep_remove entry."""
    
    def __init__(self):
        super().__init__("__ep_remove", internal=False)
        self.epoll_hit = False
        
    def stop(self):
        global inner_epoll_addr, epi_addr, thread_a_trapped
        
        ep = int(gdb.parse_and_eval("$x0"))
        epi = int(gdb.parse_and_eval("$x1"))
        
        # epi->ffd.file at offset 48 (from disassembly: ldr x24, [x1, #48])
        file_ptr = int(gdb.parse_and_eval(f"*(unsigned long*)({epi} + 48)"))
        
        # Check if this is an epoll file
        f_op = int(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + {f_op_offset})"))
        is_epoll = (f_op == eventpoll_fops_addr)
        
        log(f"__ep_remove: ep={hex(ep)}, epi={hex(epi)}, file={hex(file_ptr)}, f_op={hex(f_op)}, is_epoll={is_epoll}")
        
        if is_epoll and not self.epoll_hit:
            self.epoll_hit = True
            epi_addr = epi
            
            # Get inner_epoll = file->private_data
            inner_epoll_addr = int(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + {private_data_offset})"))
            log(f"  >> EPOLL FILE DETECTED!")
            log(f"  >> inner_epoll = file->private_data = {hex(inner_epoll_addr)}")
            
            # Get f_ep
            f_ep = int(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + {f_ep_offset})"))
            log(f"  >> file->f_ep = {hex(f_ep)}")
            log(f"  >> &inner_epoll->refs = {hex(inner_epoll_addr + refs_offset)}")
            
            # Read refs.first
            refs_first = int(gdb.parse_and_eval(f"*(unsigned long*)({inner_epoll_addr + refs_offset})"))
            log(f"  >> inner_epoll->refs.first (offset {refs_offset}) = {hex(refs_first)}")
            refs_first_history.append(("AT_ENTRY", refs_first))
            
            # Count epitems in refs
            count = 0
            node = refs_first
            while node != 0 and count < 10:
                count += 1
                node_next = int(gdb.parse_and_eval(f"*(unsigned long*)({node})"))
                log(f"    refs[{count}]: {hex(node)} -> next={hex(node_next)}")
                node = node_next
            log(f"  >> Epitems in inner_epoll->refs: {count}")
            
            # Check the condition: is this single-epitem? (line 826)
            # head->first == &epi->fllink && !epi->fllink.next
            epi_fllink_addr = epi + 80  # fllink at offset 80 in epitem
            fllink_next = int(gdb.parse_and_eval(f"*(unsigned long*)({epi} + 80)"))
            log(f"  >> epi->fllink at {hex(epi_fllink_addr)}, fllink.next = {hex(fllink_next)}")
            
            is_single = (refs_first == epi_fllink_addr and fllink_next == 0)
            log(f"  >> Single-epitem check: head->first==&epi->fllink={refs_first == epi_fllink_addr}, !fllink.next={fllink_next == 0}")
            log(f"  >> Would WRITE_ONCE(f_ep, NULL) execute? {is_single}")
            
            if not is_single:
                log(f"  >> DUAL-WATCH CONFIRMED: f_ep will NOT be set NULL")
                log(f"  >> The lockless-bypass race (VER-026) CANNOT trigger")
                log(f"  >> inner_epoll will NOT be freed before hlist_del_rcu")
            
            # Full dump BEFORE
            log(f"  >> Full dump of inner_epoll at {hex(inner_epoll_addr)} (BEFORE hlist_del_rcu):")
            try:
                mem = gdb.execute(f"x/24gx {inner_epoll_addr}", to_string=True)
                log("\n" + mem)
            except Exception as e:
                log(f"  Error: {e}")
            
            # Set hardware watchpoint on inner_epoll->refs.first
            watch_addr = inner_epoll_addr + refs_offset
            log(f"  >> Setting hardware watchpoint on {hex(watch_addr)}")
            try:
                WatchRefsFirst(inner_epoll_addr, refs_offset)
            except Exception as e:
                log(f"  >> Watchpoint failed: {e}")
                log(f"  >> Falling back to instruction breakpoints")
                # Use instruction breakpoints instead
                # Path 1: __ep_remove + 0xb0 (str x2, [x0] for non-head case)
                # Path 2: __ep_remove + 0x198 (str x2, [x0] for head case)
                # +0x19c is the instruction AFTER the head-case write
                HlistWriteBreak(inner_epoll_addr, refs_offset, 0xb0)
                HlistWriteBreak(inner_epoll_addr, refs_offset, 0x198)
            
            # Set ep_free breakpoint
            EpFreeBreak(inner_epoll_addr)
            
            self.enabled = False
        
        return False

class WatchRefsFirst(gdb.Breakpoint):
    """Hardware watchpoint on inner_epoll->refs.first."""
    
    def __init__(self, inner_ep, offset):
        addr = inner_ep + offset
        # Use awatch (access watchpoint) for writes
        super().__init__(f"*(unsigned long*)({addr})", type=gdb.BP_WATCHPOINT, wp_class=gdb.WP_WRITE, internal=False)
        self.inner_ep = inner_ep
        self.offset = offset
        
    def stop(self):
        global write_captured
        
        new_val = int(gdb.parse_and_eval(f"*(unsigned long*)({self.inner_ep + self.offset})"))
        
        log(f"=== WATCHPOINT FIRED: inner_epoll+{self.offset} modified! ===")
        log(f"  New value at offset {self.offset}: {hex(new_val)}")
        refs_first_history.append(("WATCHPOINT_WRITE", new_val))
        
        # Backtrace
        try:
            bt = gdb.execute("bt 5", to_string=True)
            log(f"  Backtrace:\n{bt}")
        except:
            pass
        
        # PC info
        pc = int(gdb.parse_and_eval("$pc"))
        ep_remove_base = int(gdb.parse_and_eval("(unsigned long)__ep_remove"))
        if pc >= ep_remove_base and pc < ep_remove_base + 0x300:
            offset_in_func = pc - ep_remove_base
            log(f"  PC = {hex(pc)} = __ep_remove + 0x{offset_in_func:x}")
        
        # Check if inner_epoll is alive
        log(f"  ep_free_inner_seen: {ep_free_inner_seen}")
        if not ep_free_inner_seen:
            log(f"  >> inner_epoll is STILL ALIVE — this is NOT a UAF write")
        else:
            log(f"  >> inner_epoll was ALREADY FREED — this IS a UAF write!")
        
        if new_val == 0:
            log(f"  >> Value is NULL — single-epitem hlist_del_rcu")
        elif (new_val & 0xffff000000000000) == 0xffff000000000000:
            log(f"  >> Value is kernel pointer — multi-epitem hlist_del_rcu")
        
        write_captured = True
        
        # Dump full inner_epoll
        log(f"  >> Full dump of inner_epoll at {hex(self.inner_ep)} (AT WRITE TIME):")
        try:
            mem = gdb.execute(f"x/24gx {self.inner_ep}", to_string=True)
            log("\n" + mem)
        except Exception as e:
            log(f"  Error: {e}")
        
        self.enabled = False
        return False

class HlistWriteBreak(gdb.Breakpoint):
    """Instruction breakpoint at hlist_del write instruction."""
    
    def __init__(self, inner_ep, offset, func_offset):
        addr = int(gdb.parse_and_eval(f"(unsigned long)__ep_remove + {func_offset}"))
        super().__init__(f"*{addr}", internal=False)
        self.inner_ep = inner_ep
        self.offset = offset
        self.func_offset = func_offset
        
    def stop(self):
        global write_captured
        
        # At the str instruction: x2 = value to write, x0 = address to write to
        target_addr = int(gdb.parse_and_eval("$x0"))
        value = int(gdb.parse_and_eval("$x2"))
        
        log(f"=== HLIST_DEL WRITE at __ep_remove+0x{self.func_offset:x} ===")
        log(f"  Writing *{hex(target_addr)} = {hex(value)}")
        
        expected_addr = self.inner_ep + self.offset
        if target_addr == expected_addr:
            log(f"  >> Target IS inner_epoll+{self.offset} (refs.first)")
            log(f"  >> ep_free_inner_seen: {ep_free_inner_seen}")
            
            if not ep_free_inner_seen:
                log(f"  >> inner_epoll is ALIVE — NOT a UAF write")
            else:
                log(f"  >> inner_epoll was FREED — THIS IS A UAF WRITE!")
            
            write_captured = True
            
            # Single-step to execute the str
            gdb.execute("stepi")
            
            # Read back
            new_val = int(gdb.parse_and_eval(f"*(unsigned long*)({expected_addr})"))
            log(f"  >> After write: inner_epoll+{self.offset} = {hex(new_val)}")
            refs_first_history.append(("HLIST_WRITE", new_val))
            
            # Dump
            log(f"  >> Full dump AFTER hlist_del_rcu:")
            try:
                mem = gdb.execute(f"x/24gx {self.inner_ep}", to_string=True)
                log("\n" + mem)
            except Exception as e:
                log(f"  Error: {e}")
        else:
            log(f"  >> Target is {hex(target_addr)}, NOT inner_epoll+{self.offset}")
            log(f"  >> This write is for a different hlist — ignoring")
        
        self.enabled = False
        return False

class EpFreeBreak(gdb.Breakpoint):
    """Break on ep_free to track if/when inner_epoll is freed."""
    
    def __init__(self, inner_ep):
        super().__init__("ep_free", internal=False)
        self.inner_ep = inner_ep
        
    def stop(self):
        global ep_free_inner_seen
        
        ep = int(gdb.parse_and_eval("$x0"))
        log(f"ep_free: ep={hex(ep)}")
        
        if ep == self.inner_ep:
            ep_free_inner_seen = True
            log(f"  >> INNER_EPOLL being freed!")
            log(f"  >> write_captured at this point: {write_captured}")
            
            if write_captured:
                log(f"  >> hlist_del_rcu already wrote to LIVE memory BEFORE free")
                log(f"  >> CONFIRMED: The write was NOT a UAF")
            else:
                log(f"  >> hlist_del_rcu has NOT fired yet")
                log(f"  >> If it fires after this, it WOULD be a UAF")
            
            refs_val = int(gdb.parse_and_eval(f"*(unsigned long*)({self.inner_ep + refs_offset})"))
            refs_first_history.append(("EP_FREE", refs_val))
        
        return False

# Final stop handler
final_done = False
def stop_handler(event):
    global final_done
    
    if isinstance(event, gdb.SignalEvent):
        if event.stop_signal == "SIGINT" and not final_done:
            final_done = True
            
            log("=" * 60)
            log("FINAL EXPERIMENT REPORT")
            log("=" * 60)
            log(f"inner_epoll_addr: {hex(inner_epoll_addr) if inner_epoll_addr else 'NOT SET'}")
            log(f"write_captured: {write_captured}")
            log(f"ep_free_inner_seen: {ep_free_inner_seen}")
            log(f"refs.first history (chronological):")
            for label, val in refs_first_history:
                log(f"  {label}: {hex(val)}")
            
            if not inner_epoll_addr:
                log("")
                log("PROBLEM: inner_epoll was never identified.")
                log("The epoll file check may have failed.")
            elif write_captured and not ep_free_inner_seen:
                log("")
                log("VERDICT: hlist_del_rcu wrote to LIVE inner_epoll (never freed during trace).")
                log("The dual-watch topology does NOT produce a UAF write.")
                log("VER-029/VER-030 claims are UNSUPPORTED by this evidence.")
            elif write_captured and ep_free_inner_seen:
                # Check ordering from history
                write_idx = next((i for i, (l, _) in enumerate(refs_first_history) if "WRITE" in l), -1)
                free_idx = next((i for i, (l, _) in enumerate(refs_first_history) if l == "EP_FREE"), -1)
                if write_idx >= 0 and free_idx >= 0:
                    if write_idx < free_idx:
                        log("")
                        log("VERDICT: Write happened BEFORE free → NOT a UAF.")
                    else:
                        log("")
                        log("VERDICT: Write happened AFTER free → IS a UAF!")
                        log("This would support VER-029/VER-030 IF the value is a kernel pointer.")
            elif not write_captured:
                log("")
                log("VERDICT: hlist_del_rcu write was not observed.")
                log("Possible: function exited early, or breakpoints didn't fire.")
            
            log("=" * 60)
            gdb.execute("quit")
        
        elif event.stop_signal in ["SIGTRAP", "SIGSEGV", "SIGBUS"]:
            log(f"CRASH: {event.stop_signal}")
            try:
                log(gdb.execute("bt", to_string=True))
            except:
                pass
            gdb.execute("quit")

gdb.events.stop.connect(stop_handler)

EpRemoveEntry()
log("EXP-024 breakpoints set. Continuing.")

def timeout():
    time.sleep(60.0)
    log("TIMEOUT (60s)")
    os.kill(os.getpid(), signal.SIGINT)

t = threading.Thread(target=timeout)
t.daemon = True
t.start()

try:
    gdb.execute("c")
except KeyboardInterrupt:
    pass
except gdb.error as e:
    log(f"GDB error: {e}")

gdb.execute("quit")
