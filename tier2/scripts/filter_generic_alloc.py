import subprocess
import re
import sys
import os

def grep_source_for_alloc(struct_name):
    # Search for allocations. We want to see if there's kmalloc/kzalloc or kmem_cache_create
    try:
        # Check for dedicated cache creation
        # Using grep to find kmem_cache_create.*"struct_name" or similar is tricky,
        # but we can search for the exact struct name near kmem_cache_create
        cmd_cache = f"grep -rE 'kmem_cache_create.*sizeof.*struct {struct_name}\\b' third_party/linux-6.12.67/ 2>/dev/null | head -n 1"
        result_cache = subprocess.run(cmd_cache, shell=True, capture_output=True, text=True)
        if result_cache.stdout.strip():
            return "kmem_cache"

        # Check for generic kmalloc/kzalloc/kvmalloc/kvzalloc
        cmd_generic = f"grep -rE 'k[vz]?m[a-z]*alloc\\(.*sizeof\\(\\*?[a-zA-Z0-9_]+\\).*' third_party/linux-6.12.67/ 2>/dev/null | grep 'struct {struct_name}\\b' | head -n 1"
        # The above is not perfect because usually the struct name is in a cast or sizeof.
        # Let's try a simpler approach: just look for sizeof(struct {struct_name}) near kmalloc
        cmd_generic2 = f"grep -rE 'k[a-z]*alloc\\(.*sizeof\\(struct {struct_name}\\)' third_party/linux-6.12.67/ 2>/dev/null | head -n 1"
        result_generic2 = subprocess.run(cmd_generic2, shell=True, capture_output=True, text=True)
        if result_generic2.stdout.strip():
            # Now we must check if it's zalloc (which zeros offset 160) or not
            if "kzalloc" in result_generic2.stdout or "kvzalloc" in result_generic2.stdout or "GFP_ZERO" in result_generic2.stdout:
                return "kzalloc (zeroing)"
            return "kmalloc (generic)"
            
        return "unknown/complex"
    except Exception as e:
        return f"ERROR"

def main():
    with open('tier2/evidence/pahole_struct_sizes.txt', 'r') as f:
        lines = f.readlines()
        
    print(f"Checking {len(lines)} candidates for generic allocation...")
    
    for line in lines:
        parts = line.strip().split()
        if len(parts) >= 2:
            struct_name = parts[1]
            size = parts[0]
            alloc_type = grep_source_for_alloc(struct_name)
            if alloc_type == "kmalloc (generic)":
                print(f"{size} {struct_name} : {alloc_type}")

if __name__ == "__main__":
    main()
