import subprocess
import re

candidates = [
    "audit_aux_data_bprm_fcaps", "rhltable", "elf_prpsinfo", "nfs_open_context",
    "p9_client", "io_futex_data", "uart_8250_em485", "ports_device", "netpoll_info",
    "ptr_ring", "log_c", "snd_pcm_sw_params", "snd_seq_port_info", "nfnl_err",
    "xfrm_userpolicy_info"
]

def check_offset_160(struct_name):
    cmd = f"pahole -F dwarf -C {struct_name} third_party/linux-6.12.67/vmlinux 2>/dev/null"
    res = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    out = res.stdout
    if not out:
        return "Failed to run pahole"
        
    lines = out.splitlines()
    field = "PADDING / END_OF_STRUCT"
    
    for i, line in enumerate(lines):
        # Look for comment at the end of the line: /*  160   8 */
        # Regex to capture offset and size
        match = re.search(r'/\*\s+(\d+)\s+(\d+)\s+\*/', line)
        if match:
            offset = int(match.group(1))
            size = int(match.group(2))
            
            # If the field spans offset 160 (e.g. starts at 152, size 16, spans 152-167)
            if offset <= 160 < offset + size:
                # The actual field is everything before the /*
                field_def = line.split('/*')[0].strip()
                field = f"Offset {offset} (size {size}): {field_def}"
                break
    return field

for cand in candidates:
    res = check_offset_160(cand)
    print(f"{cand:<30}: {res}")

