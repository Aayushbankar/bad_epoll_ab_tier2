import gdb
import json

def get_offset(struct, field):
    try:
        t = gdb.lookup_type(f"struct {struct}")
        for f in t.fields():
            if f.name == field:
                return f.bitpos // 8
    except:
        return -1

def get_size(struct):
    try:
        t = gdb.lookup_type(f"struct {struct}")
        return t.sizeof
    except:
        return -1

info = {
    "task_struct": {
        "comm": get_offset("task_struct", "comm"),
        "children": get_offset("task_struct", "children"),
        "sibling": get_offset("task_struct", "sibling"),
        "sas_ss_sp": get_offset("task_struct", "sas_ss_sp"),
        "files": get_offset("task_struct", "files"),
    },
    "file": {
        "sizeof": get_size("file"),
        "f_count": get_offset("file", "f_count"),
        "f_op": get_offset("file", "f_op"),
        "private_data": get_offset("file", "private_data"),
        "f_inode": get_offset("file", "f_inode")
    },
    "file_operations": {
        "poll": get_offset("file_operations", "poll")
    },
    "inode": {
        "i_sb": get_offset("inode", "i_sb"),
        "i_ino": get_offset("inode", "i_ino")
    },
    "files_struct": {
        "fdt": get_offset("files_struct", "fdt")
    },
    "fdtable": {
        "fd": get_offset("fdtable", "fd")
    },
    "pipe_inode_info": {
        "bufs": get_offset("pipe_inode_info", "bufs")
    },
    "pipe_buffer": {
        "page": get_offset("pipe_buffer", "page")
    }
}
with open("/tmp/offsets.json", "w") as f:
    json.dump(info, f, indent=4)
gdb.execute("quit")
