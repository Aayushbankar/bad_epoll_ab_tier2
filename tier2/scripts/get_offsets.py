import gdb
gdb.execute("file tier2/android/artifacts/vmlinux")
def p_offset(type, field):
    try:
        out = gdb.execute(f"print &((({type} *)0)->{field})", to_string=True)
        print(f"{type}->{field}: {out.strip().split()[-1]}")
    except:
        pass
p_offset("struct task_struct", "tasks")
p_offset("struct task_struct", "comm")
p_offset("struct task_struct", "real_cred")
p_offset("struct task_struct", "cred")
p_offset("struct cred", "uid")
p_offset("struct cred", "euid")
p_offset("struct list_head", "next")
