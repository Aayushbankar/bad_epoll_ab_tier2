import gdb
import time
import re

def setup():
    gdb.execute("set debuginfod enabled off")
    gdb.execute("set pagination off")
    gdb.execute("file linux-6.12.67/vmlinux")
    
    virt = None
    print("[*] Waiting for virt in QEMU log...")
    for _ in range(40):
        try:
            with open("qemu_output.log", "r") as f:
                content = f.read()
                m = re.search(r"virt=([0-9a-f]+)", content)
                if m:
                    virt = int(m.group(1), 16)
                    break
        except Exception as e:
            pass
        time.sleep(1)
        
    if not virt:
        print("Failed to find virt!")
        gdb.execute("quit")
        return
        
    print(f"[*] Found virt: {hex(virt)}")
    time.sleep(1)
    
    try:
        gdb.execute("target remote localhost:1234")
    except Exception as e:
        print("Failed to connect:", e)
        return

    # Check virt memory (Req 2 and 3)
    try:
        val = gdb.execute(f"x/gx {hex(virt + 0x120)}", to_string=True)
        print("[*] MEMORY AT VIRT+0x120:", val.strip())
    except Exception as e:
        print("[*] Failed to read virt memory:", e)
        
    # PIVOT4 is 0xffffffff82052ad4
    gdb.execute("hb *0xffffffff82052ad4")
    gdb.execute("c")
    
    # We hit PIVOT4: push rcx; pop rsp; pop rcx; jmp __x86_return_thunk
    gdb.execute("stepi") # push rcx
    gdb.execute("stepi") # pop rsp
    gdb.execute("stepi") # pop rcx
    
    # Now we are before jmp __x86_return_thunk.
    # Req 4: The value located at RSP immediately before __x86_return_thunk executes
    val = gdb.execute("x/gx $rsp", to_string=True)
    print("[*] MEMORY AT RSP BEFORE RETURN THUNK:", val.strip())
    
    # Step into __x86_return_thunk and until we exit it
    print("[*] Stepping until RIP changes significantly...")
    for _ in range(20):
        gdb.execute("stepi")
        pc_str = gdb.execute("print/x $pc", to_string=True)
        pc = int(pc_str.split("=")[1].strip(), 16)
        
        # __x86_return_thunk is a small thunk. If we jump to 0xffffffff810001bd, we exited it!
        # Let's say if we are not in the 0xffffffff820d.... range
        if (pc & 0xfffffffffff00000) != 0xffffffff82000000:
            break

    # Req 5: The value popped into RIP
    val = gdb.execute("print/x $pc", to_string=True)
    print("[*] RIP VALUE POPPED:", val.strip())
    
    # Req 6: First instruction executed
    val = gdb.execute("x/i $pc", to_string=True)
    print("[*] EXECUTED INSTRUCTION:", val.strip())
    
    gdb.execute("quit")

setup()
