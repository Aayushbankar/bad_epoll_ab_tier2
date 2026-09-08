import gdb

gdb.execute("target remote :1234")
try:
    gdb.execute("dump memory tier2/kallsyms_dump.txt 0xffffffc00973dac0 0xffffffc00973e258")
except Exception as e:
    print(e)
gdb.execute("quit")
