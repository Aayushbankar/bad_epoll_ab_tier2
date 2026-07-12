import gdb
import time
import re

def setup():
    gdb.execute("set debuginfod enabled off")
    gdb.execute("set pagination off")
    
    try:
        gdb.execute("target remote localhost:1234")
    except Exception as e:
        print("Failed to connect:", e)
        return

    gdb.execute("file linux-6.12.67/vmlinux")
    
    # 1. Break at rop_build_privesc return
    gdb.execute("hb *0x40dd0a")
    gdb.execute("c")
    
    try:
        val = gdb.execute("x/gx *(uint64_t*)$rax", to_string=True)
        print("[*] EVIDENCE 1: kernel_rop[0] =", val.strip())
    except Exception as e:
        print("[!] Failed to get kernel_rop[0]:", e)

    gdb.execute("delete 1")

    # 2 & 3. Break at epoll_wait
    gdb.execute("hb *__x64_sys_epoll_wait")
    
    while True:
        gdb.execute("c")
        
        # We hit epoll_wait. Check if READY_FOR_GDB is in log.
        virt = None
        try:
            with open("qemu_output.log", "r") as f:
                content = f.read()
                if "READY_FOR_GDB" in content:
                    m = re.search(r"virt=([0-9a-f]+)", content)
                    if m:
                        virt = int(m.group(1), 16)
        except Exception as e:
            pass
            
        if virt:
            break
        # If not virt, it means some other process called epoll_wait, continue.

    if virt:
        print(f"[*] Parsed virt from log: {hex(virt)}")
        try:
            val = gdb.execute(f"x/gx {hex(virt + 0x120)}", to_string=True)
            print("[*] EVIDENCE 2 & 3: virt+0x120 =", val.strip())
        except Exception as e:
            print("[!] Failed to get virt+0x120:", e)
    else:
        print("[!] Could not parse virt from qemu_output.log!")

    gdb.execute("delete 2")

    # 4 & 5. Break at PIVOT4
    gdb.execute("hb *0xffffffff82052ad4")
    gdb.execute("c")
    
    gdb.execute("stepi") # push rcx
    gdb.execute("stepi") # pop rsp
    gdb.execute("stepi") # pop rcx
    
    val = gdb.execute("x/gx $rsp", to_string=True)
    print("[*] EVIDENCE 4: RSP BEFORE JMP/RET =", val.strip())
    
    for _ in range(20):
        gdb.execute("stepi")
        pc_str = gdb.execute("print/x $pc", to_string=True)
        pc = int(pc_str.split("=")[1].strip(), 16)
        if (pc & 0xfffffffffff00000) != 0xffffffff82000000:
            break
            
    val = gdb.execute("print/x $pc", to_string=True)
    print("[*] EVIDENCE 5: RIP EXECUTED =", val.strip())
    
    val = gdb.execute("x/i $pc", to_string=True)
    print("[*] EVIDENCE 6: EXECUTED INSTRUCTION =", val.strip())
    
    gdb.execute("quit")

setup()
