import gdb

def run_experiment():
    print("\n=== STARTING EXP-008 TIMING & RECLAIM EXPERIMENT ===\n")
    
    # Target break addresses:
    # 1. list_del_init in __ep_remove on the rdllink
    # Note: 0xffffffc0083bcf3c is a call to __list_del_entry_valid. 
    # Just before it, x19 contains &epi->rdllink (offset 24).
    # Then there's the manual inline list_del at 0xffffffc0083bcf44
    # The actual writes are:
    #   0xffffffc0083bcf50: str x19, [x20, #24]  ; epi->rdllink.next = LIST_POISON1
    #   0xffffffc0083bcf54: str x19, [x20, #32]  ; epi->rdllink.prev = LIST_POISON2
    # So we break at the call to __list_del_entry_valid (0xffffffc0083bcf3c)
    # x20 contains epi.
    list_del_site = 0xffffffc0083bcf3c
    
    # 2. call_rcu in __ep_remove (so we know when it's freed)
    call_rcu_site = 0xffffffc0083bcf74
    
    # 3. do_epoll_ctl kmem_cache_alloc return for new epitem
    alloc_ret_site = 0xffffffc0083be4d8
    
    gdb.execute("set pagination off")
    gdb.execute("set confirm off")
    gdb.execute("target remote :1234")
    
    gdb.execute(f"break *{list_del_site}")
    gdb.execute(f"break *{call_rcu_site}")
    gdb.execute(f"break *{alloc_ret_site}")
    
    print(f"[*] Breakpoints set:")
    print(f"    - list_del_init: {hex(list_del_site)}")
    print(f"    - call_rcu:      {hex(call_rcu_site)}")
    print(f"    - kmem_cache_zalloc return: {hex(alloc_ret_site)}")
    
    freed_epi_addr = None
    list_del_hit = False
    list_del_target = None
    allocated_epis = []
    
    while True:
        try:
            gdb.execute("continue")
        except gdb.error:
            break
            
        pc = int(gdb.parse_and_eval("$pc")) & 0xffffffffffffffff
        
        if pc == list_del_site:
            # x20 contains the epi being operated on
            list_del_target = int(gdb.parse_and_eval("$x20")) & 0xffffffffffffffff
            print(f"[EVENT] list_del_init hit for epi: {hex(list_del_target)}")
            list_del_hit = True
            
        elif pc == call_rcu_site:
            if not freed_epi_addr:
                # x20 contains the epi being freed via RCU
                freed_epi_addr = int(gdb.parse_and_eval("$x20")) & 0xffffffffffffffff
                print(f"[EVENT] __ep_remove call_rcu(epi) hit for epi: {hex(freed_epi_addr)}")
                
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
                    
                    if list_del_hit and list_del_target == freed_epi_addr:
                        print("\n[TIMING CONCLUSION]")
                        print("The list_del_init on the victim epi occurred BEFORE reclaim.")
                        print("The corrupting write lands on STALE FREED MEMORY before it is reallocated.")
                    else:
                        print("\n[TIMING CONCLUSION]")
                        print("The list_del_init did NOT occur before reclaim.")
                    break
    
    print("\n=== EXPERIMENT COMPLETE ===")
    
if __name__ == "__main__":
    run_experiment()
