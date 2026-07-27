import gdb

def run_experiment():
    print("\n=== STARTING EXP-007 SAME-CACHE RECLAIM EXPERIMENT ===\n")
    
    # Target break addresses:
    # __ep_remove calls call_rcu (so we can get the freed epitem)
    # The epitem pointer in __ep_remove is in x20 when call_rcu is called
    call_rcu_site = 0xffffffc0083bcf74
    
    # do_epoll_ctl kmem_cache_alloc return for new epitem
    alloc_ret_site = 0xffffffc0083be4d8
    
    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute("target remote :1234")
    
    gdb.execute(f"break *{call_rcu_site}")
    gdb.execute(f"break *{alloc_ret_site}")
    
    print("[*] Breakpoints set on __ep_remove's call_rcu and do_epoll_ctl's kmem_cache_alloc return. Continuing...")
    
    freed_epi_addr = None
    allocated_epis = []
    
    while True:
        try:
            gdb.execute("continue")
        except gdb.error:
            break
            
        pc = int(gdb.parse_and_eval("$pc")) & 0xffffffffffffffff
        
        if pc == call_rcu_site:
            if not freed_epi_addr:
                # x20 contains the epi being freed via RCU
                freed_epi_addr = int(gdb.parse_and_eval("$x20")) & 0xffffffffffffffff
                print(f"[EVIDENCE 1] Breakpoint hit at __ep_remove call_rcu(epi, ...)")
                print(f"             Freed epitem address: {hex(freed_epi_addr)}")
        elif pc == alloc_ret_site:
            if freed_epi_addr:  # Only track allocations after the free
                allocated_epi = int(gdb.parse_and_eval("$x0")) & 0xffffffffffffffff
                allocated_epis.append(allocated_epi)
                
                # Check for match immediately
                if allocated_epi == freed_epi_addr:
                    print(f"\n[MATCH] EXACT POINTER REUSE CONFIRMED!")
                    print(f"        Freed epitem:     {hex(freed_epi_addr)}")
                    print(f"        Allocated epitem: {hex(allocated_epi)}")
                    print(f"        Reclaim occurred on allocation #{len(allocated_epis)}")
                    break
    
    print("\n=== EXPERIMENT COMPLETE ===")
    if freed_epi_addr and freed_epi_addr in allocated_epis:
        print("RESULT: PASSED (Same-cache reclaim verified)")
    else:
        print("RESULT: FAILED (No address match)")
        print(f"Total new allocations tracked: {len(allocated_epis)}")
        
if __name__ == "__main__":
    run_experiment()
