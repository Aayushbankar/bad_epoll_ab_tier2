import gdb

gdb.execute("set pagination off")
gdb.execute("target remote :1234")

# Set a breakpoint on start_kernel to see if it even reaches there
# Note: start_kernel might be relocated, but let's try. Actually, start_kernel might not be relocated YET, or maybe it is.
# The primary entry point `_text` is un-relocated at boot.
gdb.execute("b *0xffff800080000000") # typical base
gdb.execute("continue")

print("At base. Now continuing to start_kernel...")
gdb.execute("b start_kernel")
gdb.execute("continue")

print("At start_kernel.")
gdb.execute("quit")
