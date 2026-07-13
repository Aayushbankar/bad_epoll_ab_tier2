import gdb
def setup():
    gdb.execute("set debuginfod enabled off")
    gdb.execute("set pagination off")
    gdb.execute("b *0x40dd0a")
    gdb.execute("run")
    val = gdb.execute("print/x *(uint64_t*)*(uint64_t*)$rax", to_string=True)
    print("[*] KERNEL_ROP[0] VALUE:", val.strip())
    gdb.execute("quit")
setup()
