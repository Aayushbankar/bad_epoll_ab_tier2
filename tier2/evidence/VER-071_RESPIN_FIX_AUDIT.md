# VER-071: CVE-2026-46242 Fix Status Across All android15-6.6 GKI Respins

> **Date**: September 13, 2026  
> **Type**: STATIC (source-level analysis via Gitiles API)  
> **Method**: Direct `fs/eventpoll.c` fetch per tag via `curl ... ?format=TEXT | base64 -d`  
> **Supersedes/Extends**: VER-070 (which only checked r1)

---

## Verification Standard

Three structural fix patterns checked per file:
1. `hlist_is_singular_node()` presence in the remove path
2. `__ep_remove` function absent (replaced by `ep_remove_file()` + `ep_remove()`)
3. `epi_fget()` called inside `ep_remove()` (not just in `ep_item_poll()`)

## Results

### Fix Introduction Points (per branch)

| Branch | Kernel | Last Vulnerable | First Fixed |
|--------|--------|-----------------|-------------|
| android15-6.6-2025-09 | 6.6.98 | r20 | **r21** |
| android15-6.6-2025-10 | 6.6.102 | **r31** | **r32** |
| android15-6.6-2026-01 | 6.6.118 | r36 | **r37** |
| android15-6.6-2026-04 | 6.6.127 | r22 | **r23** |
| android15-6.6-2026-07 | 6.6.139 | r1 | **r2** |

### Key Observation

None of these branches reach kernel 6.6.144 (the LTS fix merge point). The highest
is 6.6.139. Google cherry-picked the full 8-commit `[PATCH 6.6.y 0/8] eventpoll: fix
ep_remove` series directly from upstream into all 5 branches.

### Cherry-Picked Commits (found in android15-6.6-2025-10_r32 log)

```
44fbcef09f1f  UPSTREAM: eventpoll: defer struct eventpoll free to RCU grace period
545b4a2b524a  UPSTREAM: eventpoll: use hlist_is_singular_node() in __ep_remove()
6654aec882f4  UPSTREAM: eventpoll: split __ep_remove()
7c59ccd799cc  UPSTREAM: eventpoll: kill __ep_remove()
b1ccd770f21b  UPSTREAM: eventpoll: rename ep_remove_safe() back to ep_remove()
3e161e6c0ed6  UPSTREAM: eventpoll: drop vestigial __ prefix from ep_remove_{file,epi}()
5598c91afdf7  UPSTREAM: eventpoll: move epi_fget() up
832bd7b37ec9  UPSTREAM: eventpoll: fix ep_remove struct eventpoll / struct file UAF
```

### VER-070 Reassessment

VER-070's conclusion that `android15-6.6-2025-10_r1` is vulnerable remains **CORRECT**.
However, VER-070 was incomplete: it did not check the latest respin (r32), which IS
fixed. VER-070 should not be cited as evidence that "all 2025-10 builds are vulnerable."

### Impact

The premise in KERNEL_PIVOT_DECISION.md that "commercial devices stay vulnerable
indefinitely because LTS 6.6.144 hasn't been merged" is **WRONG**. Google patched
all active branches via out-of-band cherry-picks. Only devices frozen on pre-fix
respins remain vulnerable.

---

## Verdict

**VERIFIED**: Fix present in latest respin of all 5 branches.  
**IMPLICATION**: Lab work targeting r1 (vulnerable) is still valid, but claims about
current-device vulnerability must be caveated.
