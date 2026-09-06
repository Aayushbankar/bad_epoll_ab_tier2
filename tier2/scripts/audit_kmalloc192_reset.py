#!/usr/bin/env python3
"""
audit_kmalloc192_reset.py — Single-pass pahole parser for kmalloc-192 repeated-reset gadgets.
Analyzes all structs sized 129–192 bytes and flags fields at offset 160 (0xa0).
"""

import subprocess
import sys
import re
import json

def classify_field(field_line):
    clean = field_line.split("/*")[0].strip()
    line_lower = clean.lower()
    
    # Check if pointer
    if "*" in line_lower:
        return "POINTER", clean
    if any(k in line_lower for k in ["atomic", "refcount"]):
        return "REFCOUNT/ATOMIC_COUNTER", clean
    if any(k in line_lower for k in ["count", "cnt", "num", "nr_", "total", "seq", "idx", "index", "ticks"]):
        return "COUNTER/INDEX", clean
    if any(k in line_lower for k in ["flag", "state", "status", "mask", "mode"]):
        return "FLAG/STATE", clean
    if any(k in line_lower for k in ["int", "short", "long", "u32", "u64", "u16", "u8", "size_t", "bool"]):
        return "INTEGER/SCALAR", clean
    return "OTHER/STRUCT", clean

def process_vmlinux(vmlinux_path):
    print(f"[*] Launching single-pass pahole on {vmlinux_path}...")
    cmd = ["pahole", "--structs", vmlinux_path]
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, bufsize=1024*1024)

    current_struct = None
    struct_lines = []
    
    candidates = []

    # Regex patterns
    struct_start_re = re.compile(r'^struct\s+([a-zA-Z0-9_]+)\s*\{')
    size_re = re.compile(r'/\*\s*size:\s*(\d+)')
    offset_re = re.compile(r'/\*\s*(\d+)(?::\s*\d+)?\s+(\d+)\s*\*/')

    for line in p.stdout:
        m_start = struct_start_re.match(line)
        if m_start:
            current_struct = m_start.group(1)
            struct_lines = [line]
            continue
        
        if current_struct:
            struct_lines.append(line)
            m_size = size_re.search(line)
            if m_size:
                size = int(m_size.group(1))
                if 128 < size <= 192:
                    # Parse lines in this struct for offset 160
                    fields_160 = []
                    for sline in struct_lines:
                        m_off = offset_re.search(sline)
                        if m_off:
                            off = int(m_off.group(1))
                            fsize = int(m_off.group(2))
                            # Check if field covers or starts in range [160, 168)
                            if (off <= 160 < off + fsize) or (160 <= off < 168):
                                fclass, clean_field = classify_field(sline)
                                fields_160.append({
                                    "offset": off,
                                    "field_size": fsize,
                                    "class": fclass,
                                    "field": clean_field,
                                    "raw": sline.strip()
                                })
                    if fields_160:
                        candidates.append({
                            "struct": current_struct,
                            "size": size,
                            "fields": fields_160
                        })
                current_struct = None
                struct_lines = []

    p.wait()
    return candidates

def main():
    target = "tier2/android/source/common/vmlinux" if len(sys.argv) < 2 else sys.argv[1]
    candidates = process_vmlinux(target)
    print(f"[*] Found {len(candidates)} structs in kmalloc-192 with fields spanning offset 160.")

    non_ptr_candidates = []
    for c in candidates:
        non_ptrs = [f for f in c["fields"] if f["class"] != "POINTER"]
        if non_ptrs:
            c_copy = dict(c)
            c_copy["fields"] = non_ptrs
            non_ptr_candidates.append(c_copy)

    print(f"[*] Structs with non-pointer fields (counters/flags/scalars) at offset 160: {len(non_ptr_candidates)}")

    print("\n" + "="*95)
    print(f"{'STRUCT NAME':<32} | {'SZ':<3} | {'OFF':<3} | {'FSZ':<3} | {'CLASS':<22} | {'FIELD DECLARATION'}")
    print("="*95)

    for c in non_ptr_candidates:
        for f in c["fields"]:
            print(f"{c['struct']:<32} | {c['size']:<3} | {f['offset']:<3} | {f['field_size']:<3} | {f['class']:<22} | {f['field']}")

    out_file = "tier2/evidence/EXP-016b/audit_gki_candidates.json"
    with open(out_file, "w") as f:
        json.dump(candidates, f, indent=2)
    print(f"\n[*] Full candidate details saved to {out_file}")

if __name__ == "__main__":
    main()
