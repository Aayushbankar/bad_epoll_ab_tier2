import gdb
import json

def get_offset(struct, field):
    try:
        t = gdb.lookup_type(f"struct {struct}")
        for f in t.fields():
            if f.name == field:
                return f.bitpos // 8
    except:
        pass
    # fallback recursive search
    try:
        t = gdb.lookup_type(f"struct {struct}")
        for f in t.fields():
            if f.is_base_class or f.name is None:
                for sub in f.type.fields():
                    if sub.name == field:
                        return (f.bitpos + sub.bitpos) // 8
    except:
        pass
    return -1

info = {
    "task_struct": {
        "stack": get_offset("task_struct", "stack"),
    }
}
print("stack offset:", info["task_struct"]["stack"])
gdb.execute("quit")
