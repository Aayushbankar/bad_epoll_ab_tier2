import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
try:
    print(gdb.execute("print &(((struct file *)0)->f_inode)", to_string=True))
except Exception as e:
    print(e)
