import subprocess
import re
import sys
import os

def grep_source_for_alloc(struct_name):
    try:
        cmd_cache = f"grep -rE 'kmem_cache_create.*\\b{struct_name}\\b' third_party/linux-6.12.67/ 2>/dev/null | head -n 1"
        result_cache = subprocess.run(cmd_cache, shell=True, capture_output=True, text=True)
        if result_cache.stdout.strip():
            return "kmem_cache"

        cmd_generic2 = f"grep -rE 'k[a-z]*alloc\\(.*sizeof\\(struct {struct_name}\\)' third_party/linux-6.12.67/ 2>/dev/null | head -n 1"
        result_generic2 = subprocess.run(cmd_generic2, shell=True, capture_output=True, text=True)
        if result_generic2.stdout.strip():
            if "kzalloc" in result_generic2.stdout or "kvzalloc" in result_generic2.stdout or "GFP_ZERO" in result_generic2.stdout:
                return "kzalloc (zeroing)"
            return "kmalloc (generic)"
            
        return "unknown/complex"
    except Exception as e:
        return f"ERROR"

def main():
    with open('tier2/evidence/candidate_alloc_filter.txt', 'r') as f:
        lines = f.readlines()
        
    candidates = []
    reading = False
    for line in lines:
        line = line.strip()
        if not line:
            continue
        if line.startswith("==="):
            if "Dead Ends" in line:
                break
            reading = True
            continue
        if reading:
            candidates.append(line)
            
    print(f"Checking {len(candidates)} candidates for generic allocation...")
    
    for cand in candidates:
        alloc_type = grep_source_for_alloc(cand)
        if alloc_type == "kmalloc (generic)":
            print(f"{cand} : {alloc_type}")

if __name__ == "__main__":
    main()
