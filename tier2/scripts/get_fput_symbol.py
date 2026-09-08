import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
try:
    print(gdb.execute("info symbol __fput", to_string=True))
except Exception as e:
    print(e)
