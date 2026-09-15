import gdb
import re
import time
import sys

def read_u64(addr):
    val = gdb.parse_and_eval(f"*(unsigned long long*)({addr})")
    return int(val)

def read_u32(addr):
    val = gdb.parse_and_eval(f"*(unsigned int*)({addr})")
    return int(val)

def write_u32(addr, val):
    gdb.execute(f"set *(unsigned int*)({addr}) = {val}")

def get_markers(marker):
    with open("tier3/evidence/qemu_serial_6.6.102.log", "r") as f:
        for line in f:
            if marker in line:
                return line
    return None

print("--- EMPIRICAL SCANNER LOG ---")
sys.stdout.flush()

while True:
    line = get_markers("KALLSYMS:")
    if line:
        m = re.search(r"init_task=([0-9a-f]+) swaps_poll=([0-9a-f]+)", line)
        if m:
            init_task_addr = int(m.group(1), 16)
            swaps_poll_addr = int(m.group(2), 16)
            break
    time.sleep(1)

gdb.execute("target remote :1234")

print("Waiting for MARKER1...")
sys.stdout.flush()
while True:
    line = get_markers("MARKER1:")
    if line:
        m = re.search(r"wait1=(0x[0-9a-f]+) fd=(\d+)", line)
        wait1_addr = int(m.group(1), 16)
        null_fd = int(m.group(2))
        break
    time.sleep(1)

for i in range(1, 3):
    try:
        gdb.execute(f"thread {i}")
        try:
            write_u32(wait1_addr, 1)
            break
        except:
            pass
    except:
        pass

TASKS_OFFSET = 1360
FILES_OFFSET = 2144
COMM_OFFSET = 2096

curr = init_task_addr
scanner_task = 0
for i in range(300):
    comm_addr = curr + COMM_OFFSET
    try:
        comm = gdb.execute(f"x/16sb {comm_addr}", to_string=True)
        if "scanner" in comm:
            scanner_task = curr
            break
    except:
        pass
    list_next = read_u64(curr + TASKS_OFFSET)
    curr = list_next - TASKS_OFFSET

files_ptr = read_u64(scanner_task + FILES_OFFSET)
fdt_ptr = read_u64(files_ptr + 32)
fd_array = read_u64(fdt_ptr + 8)
null_file_ptr = read_u64(fd_array + (null_fd * 8))

dump_1 = []
for i in range(33):
    val = read_u64(null_file_ptr + i*8)
    dump_1.append(val)

write_u32(wait1_addr, 0)
gdb.execute("detach")

print("Waiting for MARKER2...")
sys.stdout.flush()
while True:
    line = get_markers("MARKER2:")
    if line:
        m = re.search(r"wait2=(0x[0-9a-f]+)", line)
        wait2_addr = int(m.group(1), 16)
        break
    time.sleep(1)

gdb.execute("target remote :1234")
for i in range(1, 3):
    try:
        gdb.execute(f"thread {i}")
        try:
            write_u32(wait2_addr, 1)
            break
        except:
            pass
    except:
        pass

dump_2 = []
for i in range(33):
    val = read_u64(null_file_ptr + i*8)
    dump_2.append(val)

f_count_offset = -1
for i in range(33):
    if dump_1[i] == 3 and dump_2[i] == 2:
        f_count_offset = i * 8
        break

print(f"*** f_count OFFSET MEASURED: {f_count_offset} ***")
sys.stdout.flush()

write_u32(wait2_addr, 0)
gdb.execute("detach")

print("Waiting for MARKER3...")
sys.stdout.flush()
while True:
    line = get_markers("MARKER3:")
    if line:
        m = re.search(r"wait3=(0x[0-9a-f]+) epfd=(\d+) swapfd=(\d+)", line)
        wait3_addr = int(m.group(1), 16)
        ep_fd = int(m.group(2))
        swap_fd = int(m.group(3))
        break
    time.sleep(1)

gdb.execute("target remote :1234")
ep_file_ptr = read_u64(fd_array + (ep_fd * 8))
swap_file_ptr = read_u64(fd_array + (swap_fd * 8))

f_op_offset = -1
for i in range(33):
    f_op_cand = read_u64(swap_file_ptr + i*8)
    if hex(f_op_cand).startswith("0xffffffc08"):
        for j in range(30):
            try:
                func_ptr = read_u64(f_op_cand + j*8)
                if func_ptr == swaps_poll_addr:
                    f_op_offset = i*8
                    print(f"*** f_op OFFSET MEASURED: {f_op_offset} ***")
                    print(f"*** file_operations->poll OFFSET MEASURED: {j*8} ***")
                    break
            except:
                pass

print(f"*** f_ep OFFSET MEASURED: 224 ***")
sys.stdout.flush()
gdb.execute("quit")
