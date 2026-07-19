import gdb
import time

def trace_hang():
    ep_remove_store = 0xffffffc008407f70
    ep_remove_spin = 0xffffffc008407f74
    spin_insn = 0x14000000
    
    gdb.execute("set pagination off")
    gdb.execute("break *0xffffffc008407f70")
    gdb.execute("continue")
    
    print("[*] Hit ep_remove store")
    gdb.execute("stepi")
    print("[*] Patching instruction to branch-to-self")
    gdb.execute(f"set {{unsigned int}}{ep_remove_spin} = {spin_insn}")
    
    # We will continue in background, wait 3 seconds, then interrupt
    print("[*] Continuing for 3 seconds to let Thread B run...")
    gdb.execute("set non-stop off")
    gdb.execute("set target-async on")
    try:
        gdb.execute("continue &")
    except gdb.error as e:
        print(f"Error starting background continue: {e}")
        # fallback if target-async fails
        pass

    # Wait for Thread B
    time.sleep(3)
    
    try:
        gdb.execute("interrupt")
    except:
        pass
        
    time.sleep(1) # wait for interrupt to process
    
    print("\n=== THREAD STATES ===")
    gdb.execute("info threads")
    print("\n=== BACKTRACES ===")
    gdb.execute("thread apply all bt")
    print("=== END ===")

try:
    trace_hang()
except Exception as e:
    print(f"Exception: {e}")
gdb.execute("quit")
