import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
try:
    print(gdb.execute("ptype ((struct inode *)0)->i_ino", to_string=True))
except Exception as e:
    print(e)
