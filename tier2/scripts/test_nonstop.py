import gdb

try:
    gdb.execute("set non-stop on")
    gdb.execute("set target-async on")
    gdb.execute("target remote :1234")
    print("NON-STOP-SUPPORTED")
    gdb.execute("detach")
    gdb.execute("quit")
except Exception as e:
    print(f"NON-STOP-ERROR: {e}")
    gdb.execute("quit")
