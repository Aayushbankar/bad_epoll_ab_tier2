# EXP-024 Results: Dual-Watch KASLR Leak Claim Re-Test

## Experiment ID: EXP-024
## Date: 2026-08-02
## Status: PASSED (Negative Result — VER-029/VER-030 RETRACTED)

---

## Objective

Re-test the contested claim from EXP-022b (VER-029/VER-030) that a dual-watch
epoll topology leaks a kernel heap pointer via `hlist_del_rcu` writing to offset
160 of a FREED `struct eventpoll`.

## Background

VER-029 claimed that adding `inner_epoll` to two outer epolls creates two epitems
in `inner_epoll->refs`, and when Thread A closes outer1, `hlist_del_rcu(&epi1->fllink)`
writes epi2's address to `inner_epoll->refs.first` (offset 160) of a **freed**
struct, readable via `msgrcv()`.

The raw evidence (EXP-022b_raw_gdb.log) had three critical problems:
1. "AFTER spray" dump was captured BEFORE `ep_free` — labels were inverted
2. "BEFORE spray" dump was captured AFTER `ep_free` — causal ordering wrong
3. Log ends in GDB error with NO `msgrcv()` readback ever captured

## Methodology

- Used GDB hardware watchpoint on `inner_epoll->refs.first` (offset 160)
- Deterministic breakpoint at `__ep_remove` entry to capture pre-state
- Watchpoint fires at the exact moment of the write
- `ep_free` breakpoint tracks whether inner_epoll is freed before/after write

## Key Evidence (quoted from `EXP-024_raw_gdb.log`)

### 1. Dual-watch topology confirmed (2 epitems)
```
[*]   >> inner_epoll->refs.first (offset 160) = 0xffff000001e6c250
[*]     refs[1]: 0xffff000001e6c250 -> next=0xffff000001e6c1d0
[*]     refs[2]: 0xffff000001e6c1d0 -> next=0x0
[*]   >> Epitems in inner_epoll->refs: 2
```

### 2. Single-epitem condition is FALSE — f_ep NOT set NULL
```
[*]   >> Single-epitem check: head->first==&epi->fllink=False, !fllink.next=True
[*]   >> Would WRITE_ONCE(f_ep, NULL) execute? False
[*]   >> DUAL-WATCH CONFIRMED: f_ep will NOT be set NULL
[*]   >> The lockless-bypass race (VER-026) CANNOT trigger
[*]   >> inner_epoll will NOT be freed before hlist_del_rcu
```

### 3. Watchpoint captures the write — to LIVE memory, value is NULL
```
[*] === WATCHPOINT FIRED: inner_epoll+160 modified! ===
[*]   New value at offset 160: 0x0
[*]   Backtrace:
#0  __hlist_del (n=0xffff000001e6c250) at ./include/linux/list.h:989
#1  hlist_del_rcu (n=0xffff000001e6c250) at ./include/linux/rculist.h:516
#2  __ep_remove (ep=0xffff0000037f00c0, epi=0xffff000001e6c200, force=0x1) at fs/eventpoll.c:836
[*]   ep_free_inner_seen: False
[*]   >> inner_epoll is STILL ALIVE — this is NOT a UAF write
[*]   >> Value is NULL — single-epitem hlist_del_rcu
```

### 4. Final verdict
```
[*] VERDICT: hlist_del_rcu wrote to LIVE inner_epoll (never freed during trace).
[*] The dual-watch topology does NOT produce a UAF write.
[*] VER-029/VER-030 claims are UNSUPPORTED by this evidence.
```

## Root Cause Analysis

The dual-watch claim had a **fundamental logical error** in the race model:

1. With **2+ epitems** in `inner_epoll->refs`, the condition at `eventpoll.c:826`:
   ```c
   if (head->first == &epi->fllink && !epi->fllink.next)
   ```
   is **FALSE** (because `epi->fllink.next` is non-NULL — there's a second epitem).

2. Therefore `WRITE_ONCE(file->f_ep, NULL)` at line 828 does **NOT execute**.

3. When Thread B closes `inner_epoll`, `eventpoll_release()` sees `f_ep != NULL`
   and calls `eventpoll_release_file()` — the **normal locked path**, not the
   lockless fast-path bypass that VER-026 demonstrated.

4. This means `inner_epoll` is **never freed before** `hlist_del_rcu` executes.
   The write lands on live, valid memory — not a UAF.

5. The EXP-022b "leaked" value (`0xffff0000037bb6d0`) was simply the pre-existing
   hlist state (epi2's fllink address) captured while inner_epoll was still alive,
   not a value written to freed memory by a UAF race.

## What About the Opposite Ordering?

Even if we could somehow trigger the single-epitem path with a dual-watch setup
(e.g., by closing outer2 first to reduce to 1 epitem), the single-epitem case
writes **NULL** to offset 160 (as confirmed by VER-026 and this experiment).
NULL is not a kernel pointer leak.

**There is no topology that simultaneously produces both conditions needed:**
- A kernel pointer value at offset 160 (requires 2+ epitems)
- A UAF (freed inner_epoll before write — requires single-epitem for f_ep=NULL)

These are **mutually exclusive**.

## Conclusion

**VER-029 and VER-030 are RETRACTED.**

The dual-watch KASLR leak via `hlist_del_rcu` is a structural impossibility:
the race conditions enabling the UAF (single-epitem → f_ep=NULL → lockless bypass)
are mutually exclusive with the conditions producing a kernel pointer write
(multi-epitem → writes next epitem's address). The only confirmed UAF write
primitive remains the NULL write at offset 160 from the single-epitem race (VER-026).
