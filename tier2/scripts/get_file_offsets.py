import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
import re
out = gdb.execute("ptype /o struct file", to_string=True)
print(out)
