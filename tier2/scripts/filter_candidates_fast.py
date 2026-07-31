import os
import subprocess

candidates = []
with open("tier2/evidence/kmalloc_192_candidates.txt", "r") as f:
    for line in f:
        parts = line.strip().split()
        if len(parts) >= 2:
            candidates.append(parts[0])

linux_dir = "third_party/linux-6.12.67"

print(f"Total candidates: {len(candidates)}")

cmd = ["grep", "-rE", "kzalloc|kmalloc|kmem_cache_alloc|kmem_cache_zalloc", linux_dir]
p = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)
stdout, _ = p.communicate()

results = {}

for line in stdout.splitlines():
    for c in candidates:
        if c in line:
            if "kzalloc" in line or "kmem_cache_zalloc" in line:
                results.setdefault(c, set()).add("zalloc")
            if "kmalloc" in line or "kmem_cache_alloc" in line:
                results.setdefault(c, set()).add("alloc")

print("=== Candidates with ONLY alloc (no zeroing) ===")
valid = []
for c, allocs in results.items():
    if "alloc" in allocs and "zalloc" not in allocs:
        valid.append(c)
        print(c)

print("\n=== Candidates with BOTH ===")
for c, allocs in results.items():
    if "alloc" in allocs and "zalloc" in allocs:
        print(c)

print("\n=== Candidates with ONLY zalloc (Dead Ends) ===")
for c, allocs in results.items():
    if "zalloc" in allocs and "alloc" not in allocs:
        print(c)
