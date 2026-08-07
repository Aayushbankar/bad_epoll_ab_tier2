import os
import re

candidates = []
with open("tier2/evidence/kmalloc_192_candidates.txt", "r") as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) >= 2:
            candidates.append(parts[0])

linux_dir = "third_party/linux-6.12.67"

print(f"Total candidates: {len(candidates)}")

results = {}

for root, _, files in os.walk(linux_dir):
    for file in files:
        if not file.endswith(".c") and not file.endswith(".h"):
            continue
        filepath = os.path.join(root, file)
        try:
            with open(filepath, "r", encoding="utf-8") as f:
                content = f.read()
                for c in candidates:
                    # Look for kmalloc.*sizeof.*struct c
                    # or kzalloc.*sizeof.*struct c
                    if c not in content:
                        continue
                        
                    # Pattern for allocation
                    if re.search(r'kzalloc\s*\(\s*sizeof\s*\(\s*struct\s+' + c + r'\s*\)', content) or \
                       re.search(r'kzalloc\s*\(\s*sizeof\s*\(\s*' + c + r'\s*\)', content) or \
                       re.search(r'kmem_cache_zalloc', content) and c in content:
                        if c not in results:
                            results[c] = set()
                        results[c].add('zalloc')
                    
                    if re.search(r'kmalloc\s*\(\s*sizeof\s*\(\s*struct\s+' + c + r'\s*\)', content) or \
                       re.search(r'kmalloc\s*\(\s*sizeof\s*\(\s*' + c + r'\s*\)', content) or \
                       re.search(r'kmem_cache_alloc', content) and c in content:
                        if c not in results:
                            results[c] = set()
                        results[c].add('alloc')
                        
        except Exception:
            pass

print("=== Candidates with alloc (no zeroing) ===")
for c, allocs in results.items():
    if 'alloc' in allocs and 'zalloc' not in allocs:
        print(c)

print("\n=== Candidates with BOTH ===")
for c, allocs in results.items():
    if 'alloc' in allocs and 'zalloc' in allocs:
        print(c)
        
print("\n=== Candidates with ONLY zalloc (Dead Ends) ===")
for c, allocs in results.items():
    if 'zalloc' in allocs and 'alloc' not in allocs:
        print(c)
