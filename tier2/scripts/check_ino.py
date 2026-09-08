import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
try:
    val = gdb.parse_and_eval("*(unsigned long*)0xffffffc00973e248")
    print(hex(int(val)))
except Exception as e:
    print(e)
