import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set logging file evidence/AND-002_raw_kaslr_on_2.log")
gdb.execute("set logging overwrite on")
gdb.execute("set logging enabled on")

def log(msg):
    print(f"[*] {msg}", flush=True)

for i in range(15):
    try:
        gdb.execute("target remote :1234")
        log("Connected to QEMU via GDB :1234")
        break
    except gdb.error:
        time.sleep(1)

# Stop the kernel
gdb.execute("interrupt")

# We know ksys_write is exported, but maybe not in GDB's un-relocated symbols.
# Let's try to get the address of the syscall table or print out memory via UART.
# Wait, let's just let it run and dump the memory manually.

def dump_and_quit():
    time.sleep(40)
    log("Time elapsed. Sending SIGINT to GDB...")
    import os
    import signal
    os.kill(os.getpid(), signal.SIGINT)

import threading
t = threading.Thread(target=dump_and_quit)
t.daemon = True
t.start()

log("Continuing execution for 40 seconds...")
try:
    gdb.execute("continue")
except Exception as e:
    log(f"Interrupted: {e}")

# Try to find the relocated global_log
try:
    # Get the un-relocated addresses
    buf_unreloc = 0x1b080
    idx_unreloc = 0x2b080
    
    # We need the slide. The kernel base is usually mapped at a 2MB aligned address.
    # We can try to scan memory for our magic signature "[NAT-005] "
    log("Scanning memory for signature...")
    
    # KASLR shifts the kernel by offset in 2MB increments
    # AArch64 kernel is usually loaded at the base physical RAM address + 2MB
    
    # Alternatively, just use GDB's find command on physical memory!
    # Physical memory base for virt is 0x40000000, 2GB size -> up to 0xc0000000
    # Search for "[NAT-005] "
    # But wait, test_nat005 is a userspace binary! 
    # Its address is NOT affected by kernel KASLR. It's affected by userspace ASLR (which is off if we build static and the kernel has randomise_va_space=0, or we can just search userspace)
    # The harness is run as init.
    
    # Let's just find it in physical memory.
    out = gdb.execute("find /b 0x40000000, 0xc0000000, 0x5b, 0x4e, 0x41, 0x54, 0x2d, 0x30, 0x30, 0x35, 0x5d, 0x20", to_string=True)
    log(f"Search results: {out}")
    if out and out.strip():
        lines = out.strip().split('\n')
        for line in lines:
            if line.startswith("0x"):
                addr = int(line.split()[0], 16)
                log(f"Found signature at {hex(addr)}. Dumping 64KB...")
                data = gdb.selected_inferior().read_memory(addr, 65536).tobytes()
                # find null terminator
                idx = data.find(b'\x00')
                if idx == -1: idx = 65536
                with open("evidence/AND-002_raw_kaslr_on_2.log", "w") as f:
                    f.write(data[:idx].decode("utf-8", errors="ignore"))
                log("Dumped successfully.")
                break
except Exception as e:
    log(f"Failed to dump global_log: {e}")

gdb.execute("detach")
gdb.execute("quit")
