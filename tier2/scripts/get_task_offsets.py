import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
try:
    comm_addr = int(gdb.parse_and_eval("&init_task.comm"))
    print(f"comm_addr: {hex(comm_addr)}")
except Exception as e:
    print(e)
