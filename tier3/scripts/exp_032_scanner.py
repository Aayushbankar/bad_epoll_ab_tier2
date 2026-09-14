import gdb
import time
import struct

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("target remote 127.0.0.1:1234")

print("--- EMPIRICAL SCANNER LOG ---")
gdb.execute("add-symbol-file tier3/rootfs/scanner")
gdb.execute("b scanner_harness.c:29") # printf("[HARNESS] Closing fd3\n")
gdb.execute("c")

print("[GDB] Hit scanner_harness.c:29!")

# Now we are in userspace. sp_el0 is the user stack pointer, NOT current!
# But wait! If we are stopped in userspace, we cannot easily read current!
# UNLESS we set a hardware breakpoint on a kernel function from here?
# No, if we just stepi (si) until we enter EL1 (PC >= 0xffffffc000000000)!
gdb.execute("b *0xffffffc080482df0") # break on fget! Wait, b might not work.
# Actually, if we are in userspace, we can just `si` until PC > 0xffffffc000000000!
print("[GDB] Stepping into kernel...")
while True:
    gdb.execute("si")
    pc = int(gdb.parse_and_eval("$pc"))
    if pc >= 0xffffffc000000000:
        break

print(f"[GDB] Entered kernel at {hex(pc)}!")
# Now current is in sp_el0!
current = int(gdb.parse_and_eval("$sp_el0"))
print(f"current = {hex(current)}")

inf = gdb.selected_inferior()

def read_u64(addr):
    return struct.unpack("<Q", inf.read_memory(addr, 8))[0]
def read_u32(addr):
    return struct.unpack("<I", inf.read_memory(addr, 4))[0]

files_offset = -1
for off in range(2000, 2500, 8):
    try:
        ptr = read_u64(current + off)
        if ptr > 0xffffffc000000000:
            fdt = read_u64(ptr + 16)
            if fdt > 0xffffffc000000000:
                max_fds = read_u32(fdt)
                if max_fds == 64 or max_fds == 128:
                    files_offset = off
                    break
    except:
        pass

print(f"*** files_offset = {files_offset}")
files_ptr = read_u64(current + files_offset)
fdt_ptr = read_u64(files_ptr + 16)
fd_array = read_u64(fdt_ptr + 8)

fd3_file = read_u64(fd_array + 3 * 8)
fd6_file = read_u64(fd_array + 6 * 8)
fd7_file = read_u64(fd_array + 7 * 8)

print(f"fd3 file (/tmp_dummy) = {hex(fd3_file)}")
print(f"fd6 file (epoll) = {hex(fd6_file)}")
print(f"fd7 file (/proc/swaps) = {hex(fd7_file)}")

file_mem = inf.read_memory(fd3_file, 512).tobytes()
for i in range(0, 512 - 7, 8):
    val = struct.unpack("<Q", file_mem[i:i+8])[0]
    if val == 3:
        print(f"*** EMPIRICAL f_count offset = {i}")

ep_mem = inf.read_memory(fd6_file, 512).tobytes()
for i in range(180, 250, 8):
    val = struct.unpack("<Q", ep_mem[i:i+8])[0]
    if val > 0xffffffc000000000:
        if abs(i - 216) < 40:
            print(f"*** EMPIRICAL private_data offset = {i}")

for i in range(0, 512 - 7, 8):
    val = struct.unpack("<Q", file_mem[i:i+8])[0]
    if val > 0xffffffc000000000:
        try:
            ffd_file = read_u64(val - 80 + 48)
            if ffd_file == fd3_file:
                print(f"*** EMPIRICAL f_ep offset = {i}")
        except:
            pass
        try:
            ffd_file = read_u64(val - 96 + 48)
            if ffd_file == fd3_file:
                print(f"*** EMPIRICAL f_ep offset = {i} (shifted)")
        except:
            pass

inode_val = struct.unpack("<Q", file_mem[184:184+8])[0]
print(f"*** EMPIRICAL f_inode offset = 184? val={hex(inode_val)}")

swap_mem = inf.read_memory(fd7_file, 512).tobytes()
print("swap file pointers (f_op/private_data):")
for i in range(180, 250, 8):
    val = struct.unpack("<Q", swap_mem[i:i+8])[0]
    if val > 0xffffffc000000000:
        print(f"  +{i}: {hex(val)}")
        if abs(i - 192) <= 32:
            print(f"*** EMPIRICAL f_op offset = {i}")

gdb.execute("quit")
