#!/bin/bash
while read -r cand; do
    if [ -z "$cand" ] || [[ "$cand" == ===* ]]; then continue; fi
    # Try finding cache create
    if grep -rE -q "kmem_cache_create.*\b${cand}\b" third_party/linux-6.12.67/ 2>/dev/null; then
        echo "$cand : kmem_cache"
    elif grep -rE -q "k[vz]?m[a-z]*alloc.*sizeof\(struct ${cand}\)" third_party/linux-6.12.67/ 2>/dev/null; then
        if grep -rE "k[vz]?m[a-z]*alloc.*sizeof\(struct ${cand}\)" third_party/linux-6.12.67/ 2>/dev/null | grep -qE "(kzalloc|kvzalloc|GFP_ZERO)"; then
            echo "$cand : kzalloc (zeroing)"
        else
            echo "$cand : kmalloc (generic)"
        fi
    else
        echo "$cand : unknown"
    fi
done < <(grep -v "^#" tier2/evidence/candidate_alloc_filter.txt | grep -v "^Total" | grep -v "Dead Ends" -B 100 | grep -v "===" | grep -v "^$")
