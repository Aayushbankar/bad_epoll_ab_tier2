import gdb

def run_experiment():
    print("\n=== STARTING SND_TIMER_USER REUSE EXPERIMENT ===\n")
    
    # Target break addresses:
    # ep_clear_and_put kfree call site: 0xffffffc0083bd1c4 (x22 = ep)
    # ep_clear_and_put kfree return site: 0xffffffc0083bd1c8
    # snd_timer_user_open kmalloc_trace return site: 0xffffffc008c13904 (x0 = tu)
    
    kfree_call = 0xffffffc0083bd1c4
    kfree_ret = 0xffffffc0083bd1c8
    snd_alloc_ret = 0xffffffc008c13904
    
    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute("target remote :1234")
    
    gdb.execute(f"break *{kfree_call}")
    gdb.execute(f"break *{snd_alloc_ret}")
    
    print("[*] Breakpoints set on kfree(ep) and kmalloc_trace(tu). Continuing execution...\n")
    
    freed_epoll_addr = None
    allocated_snd_addr = None
    
    # Loop to capture events
    for step in range(2):
        gdb.execute("continue")
        pc = int(gdb.parse_and_eval("$pc")) & 0xffffffffffffffff
        
        if pc == kfree_call:
            freed_epoll_addr = int(gdb.parse_and_eval("$x22")) & 0xffffffffffffffff
            print(f"[EVIDENCE 1] Breakpoint hit at ep_clear_and_put kfree(ep)")
            print(f"             Freed inner_epoll chunk address: {hex(freed_epoll_addr)}")
        elif pc == snd_alloc_ret:
            allocated_snd_addr = int(gdb.parse_and_eval("$x0")) & 0xffffffffffffffff
            print(f"[EVIDENCE 2] Breakpoint hit at snd_timer_user_open kmalloc return")
            print(f"             Returned snd_timer_user chunk address: {hex(allocated_snd_addr)}")
            
    print("\n=== REUSE VERIFICATION RESULT ===")
    print(f"Freed inner_epoll address:     {hex(freed_epoll_addr) if freed_epoll_addr else 'None'}")
    print(f"Allocated snd_timer_user address: {hex(allocated_snd_addr) if allocated_snd_addr else 'None'}")
    
    if freed_epoll_addr and allocated_snd_addr:
        if freed_epoll_addr == allocated_snd_addr:
            print(f"\n[MATCH] EXACT POINTER REUSE CONFIRMED: {hex(freed_epoll_addr)} == {hex(allocated_snd_addr)}")
        else:
            print(f"\n[MISMATCH] Address difference: Delta = {allocated_snd_addr - freed_epoll_addr:#x}")
            
if __name__ == "__main__":
    run_experiment()
