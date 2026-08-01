import os
import re
import subprocess
import sys
from collections import defaultdict

LINUX_DIR = 'third_party/linux-6.12.67'

def get_candidates():
    candidates = []
    # Read from the pre-filtered list of 330 candidates 
    # (actually wait, candidate_alloc_filter.txt has them formatted under headers)
    # Let's just read the original file that has just the names we care about?
    # Actually, we can just use the pahole_struct_sizes.txt and filter for size 129-192
    with open('tier2/evidence/pahole_struct_sizes.txt', 'r') as f:
        for line in f:
            parts = line.strip().split()
            if len(parts) >= 2:
                name = parts[0]
                size = int(parts[1])
                if 129 <= size <= 192:
                    candidates.append(name)
    return candidates

def find_dedicated_caches(candidates):
    print("Finding dedicated caches...")
    cache_structs = set()
    cmd = f"git -C {LINUX_DIR} grep -E 'kmem_cache_create' -- '*.c' 2>/dev/null"
    try:
        out = subprocess.check_output(cmd, shell=True, text=True)
        for line in out.splitlines():
            for cand in candidates:
                if f"\"{cand}\"" in line or f"sizeof(struct {cand})" in line or f"sizeof({cand})" in line:
                    cache_structs.add(cand)
    except subprocess.CalledProcessError:
        pass
    return cache_structs

def find_allocations(candidates, cache_structs):
    print(f"Finding generic allocations for {len(candidates)} candidates...")
    
    results = {} 
    
    for idx, cand in enumerate(candidates):
        if cand in cache_structs:
            results[cand] = "kmem_cache"
            continue
            
        print(f"[{idx+1}/{len(candidates)}] Analyzing struct {cand}...")
        sys.stdout.flush()
        
        # Only search inside .c files that contain the word 'alloc' and 'struct cand'
        cmd = f"git -C {LINUX_DIR} grep -l 'struct {cand}' -- '*.c' 2>/dev/null | xargs -I{{}} git -C {LINUX_DIR} grep -l 'alloc' {{}} 2>/dev/null"
        try:
            files_rel = subprocess.check_output(cmd, shell=True, text=True).splitlines()
            files = [os.path.join(LINUX_DIR, f) for f in files_rel]
        except subprocess.CalledProcessError:
            results[cand] = "none"
            continue
            
        has_generic = False
        has_zeroing = False
        
        # Regex for variable declaration: struct name *varname
        # Note: can be multiple variables on one line, or multiline, but we'll do our best with simple regex
        decl_re = re.compile(rf"struct\s+{cand}\s+\*([a-zA-Z0-9_]+)")
        
        for filepath in files:
            try:
                with open(filepath, 'r', errors='ignore') as f:
                    content = f.read()
            except:
                continue
                
            # Find all variable names of this struct type
            var_names = set()
            for match in decl_re.finditer(content):
                var_names.add(match.group(1))
                
            # Also common: struct cand *var = kmalloc(...)
            # Let's check for direct assignments to these variables
            for var in var_names:
                # var = kmalloc(..., GFP_KERNEL)
                # var = kzalloc(...)
                
                # We'll look for lines assigning to this var
                # Regex: var \s*=\s*(.*?alloc)
                assign_re = re.compile(rf"\b{var}\s*=\s*([a-zA-Z0-9_]*alloc[^\;]*)")
                for amatch in assign_re.finditer(content):
                    alloc_str = amatch.group(1)
                    if "kmem_cache_alloc" in alloc_str:
                        continue # Might be using a different cache, or we missed it
                    
                    is_zeroing = ("kzalloc" in alloc_str or "kvzalloc" in alloc_str or 
                                  "kcalloc" in alloc_str or "kvcalloc" in alloc_str or
                                  "GFP_ZERO" in alloc_str or "__GFP_ZERO" in alloc_str)
                                  
                    is_generic = ("kmalloc" in alloc_str or "kvmalloc" in alloc_str)
                    
                    if is_zeroing:
                        has_zeroing = True
                    elif is_generic:
                        has_generic = True
                        
            # Also check for direct sizeof allocations just in case
            sizeof_re = re.compile(rf"([a-zA-Z0-9_]*alloc)[^\;]*sizeof\s*\(\s*struct\s+{cand}\s*\)")
            for smatch in sizeof_re.finditer(content):
                alloc_str = smatch.group(0)
                if "kmem_cache_alloc" in alloc_str:
                    continue
                is_zeroing = ("kzalloc" in alloc_str or "kvzalloc" in alloc_str or 
                                "kcalloc" in alloc_str or "kvcalloc" in alloc_str or
                                "GFP_ZERO" in alloc_str or "__GFP_ZERO" in alloc_str)
                is_generic = ("kmalloc" in alloc_str or "kvmalloc" in alloc_str)
                
                if is_zeroing:
                    has_zeroing = True
                elif is_generic:
                    has_generic = True
                    
        if has_generic:
            results[cand] = "kmalloc (generic)"
        elif has_zeroing:
            results[cand] = "kzalloc (zeroing)"
        else:
            results[cand] = "none"
            
    return results

def main():
    candidates = get_candidates()
    cache_structs = find_dedicated_caches(candidates)
    
    results = find_allocations(candidates, cache_structs)
    
    with open('tier2/evidence/semantic_generic_kmalloc_candidates.txt', 'w') as f:
        f.write("=== GENERIC NON-ZEROING (kmalloc) ===\n")
        for cand, res in results.items():
            if res == "kmalloc (generic)":
                f.write(f"{cand}\n")
                
        f.write("\n=== GENERIC ZEROING (kzalloc) ===\n")
        for cand, res in results.items():
            if res == "kzalloc (zeroing)":
                f.write(f"{cand}\n")
                
        f.write("\n=== DEDICATED CACHE ===\n")
        for cand, res in results.items():
            if res == "kmem_cache":
                f.write(f"{cand}\n")
                
    print("Done. Results written to tier2/evidence/semantic_generic_kmalloc_candidates.txt")

if __name__ == "__main__":
    main()
