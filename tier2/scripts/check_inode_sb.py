import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
try:
    print(gdb.execute("print &(((struct inode *)0)->i_sb)", to_string=True))
except Exception as e:
    print(e)
