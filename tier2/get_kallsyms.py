import gdb
import time

gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("target remote :1234")

gdb.execute("continue") # will hit exception if something crashes, or we interrupt later.

# we can't easily wait for boot. Let's just interrupt after 10s.
