import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
try:
    print(gdb.execute("info symbol __arm64_sys_getpid", to_string=True))
except Exception as e:
    print(e)
