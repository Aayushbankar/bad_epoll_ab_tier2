import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
def p_offset(type, field):
    try:
        out = gdb.execute(f"print &((({type} *)0)->{field})", to_string=True)
        print(f"{type}->{field}: {out.strip().split()[-1]}")
    except:
        pass
p_offset("struct seq_file", "poll_event")
