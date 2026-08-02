# NAT-002: Preemption Point Analysis — Static Audit of cond_resched at Line 903 in Multi-Epitem Topology

## Objective
Identify whether a natural preemption point exists in the epoll UAF race window that can be reached without debugger assistance, specifically focusing on the `cond_resched()` at line 903 of `ep_clear_and_put` (second loop calling `ep_remove_safe` → `__ep_remove`) in a multi-epitem topology.

## Methodology
1. Source code audit of `fs/eventpoll.c` lines 870-909 (`ep_clear_and_put`)
2. Disassembly analysis of `ep_clear_and_put`, `ep_remove_safe`, and `__ep_remove` from `vmlinux` (linux-6.12.67, commit 7e35917775b8)
3. Race window analysis: mapping preemption points to the vulnerable instruction sequence in `__ep_remove`

## Source Code Audit

### ep_clear_and_put (lines 870-909)

**First Loop (lines 884-889): Poll Callback Unregistration**
```c
for (rbp = rb_first_cached(&ep->rbr); rbp; rbp = rb_next(rbp)) {
    epi = rb_entry(rbp, struct epitem, rbn);
    ep_unregister_pollwait(ep, epi);
    cond_resched();  // LINE 888
}
```

**Second Loop (lines 899-904): Epitem Removal**
```c
for (rbp = rb_first_cached(&ep->rbr); rbp; rbp = next) {
    next = rb_next(rbp);
    epi = rb_entry(rbp, struct epitem, rbn);
    ep_remove_safe(ep, epi);  // calls __ep_remove
    cond_resched();  // LINE 903
}
```

### __ep_remove (lines 804-859) — Vulnerable Function
Key instruction sequence (no preemption points inside):
```c
// Line 815: ep_unregister_pollwait(ep, epi);  // waitqueue cleanup
// Line 818-822: spin_lock(&file->f_lock); dying check
// Line 826-835: head = file->f_ep; single-epitem check; WRITE_ONCE(file->f_ep, NULL);
// Line 836: hlist_del_rcu(&epi->fllink);  // UAF WRITE: writes NULL to inner_epoll->refs.first (offset 160)
// Line 837: spin_unlock(&file->f_lock);
// Line 840: rb_erase_cached(&epi->rbn, &ep->rbr);  // operates on OUTER epoll
// Line 842-845: spin_lock_irq/spin_unlock_irq on ep->lock (OUTER epoll)
// Line 855: kfree_rcu(epi, rcu);
// Line 857: percpu_counter_dec(&ep->user->epoll_watches);  // ep = OUTER epoll (valid)
```

**Critical Finding**: `__ep_remove` contains **ZERO** `cond_resched()`, `might_resched()`, `preempt_enable()`, or any voluntary preemption point. Confirmed by source audit and disassembly.

### Disassembly Confirmation (vmlinux: ffff8000802bc824 <__ep_remove>)

```
ffff8000802bc824 <__ep_remove>:
  ... prologue ...
  ffff8000802bc858:  bl  ffff8000800cd69c <__rcu_read_lock>
  ffff8000802bc85c:  ... ep_unregister_pollwait via remove_wait_queue ...
  ffff8000802bc880:  bl  ffff8000800d36c8 <__rcu_read_unlock>
  ffff8000802bc884:  bl  ffff80008023299c <kmem_cache_free>
  ffff8000802bc898:  ... spin_lock (file->f_lock) ...
  ffff8000802bc8a0:  bl  ffff800080ccf940 <_raw_spin_lock>
  ffff8000802bc8b8:  ldr x1, [x23, #144]      # file->f_ep
  ffff8000802bc8bc:  ... WRITE_ONCE(file->f_ep, NULL) logic ...
  ffff8000802bc8d0:  ... hlist_del_rcu(&epi->fllink) logic ...
  ffff8000802bc8ec:  bl  ffff800080ccf4f0 <_raw_spin_unlock>
  ffff8000802bc8f0:  ... rb_erase_cached ...
  ffff8000802bc918:  bl  ffff800080cb4ef4 <rb_erase>
  ffff8000802bc920:  bl  ffff800080ccfa9c <_raw_spin_lock_irq>
  ffff8000802bc950:  bl  ffff800080ccf5f8 <_raw_spin_unlock_irq>
  ffff8000802bc954:  bl  ffff8000806baae8 <wakeup_source_unregister>
  ffff8000802bc968:  bl  ffff8000800d48a4 <kvfree_call_rcu>
  ffff8000802bc970:  ldr x0, [x22, #136]      # ep->user (OUTER epoll)
  ffff8000802bc974:  bl  ffff80008052f5d8 <percpu_counter_add_batch>
  ... epilogue ...
```

**No `bl <cond_resched>` or `bl <dynamic_cond_resched>` inside `__ep_remove`**. Confirmed.

### ep_clear_and_put Disassembly (ffff8000802bca90)

**First Loop cond_resched (Line 888):**
```
ffff8000802bcb30:  bl  ffff800080cc8248 <dynamic_cond_resched>  // after ep_unregister_pollwait
```

**Second Loop — ep_remove_safe call + cond_resched (Line 903):**
```
ffff8000802bcb68:  bl  ffff8000802bca44 <ep_remove_safe>        # calls __ep_remove
ffff8000802bcb6c:  bl  ffff800080cc8248 <dynamic_cond_resched>  # LINE 903 cond_resched
```

**ep_remove_safe (ffff8000802bca44):**
```
ffff8000802bca44 <ep_remove_safe>:
  ...
  ffff8000802bca5c:  bl  ffff8000802bc824 <__ep_remove>        # vulnerable function
  ...
```

## Race Window Analysis

### VER-026 Race Recap (GDB-Forced)
- Thread A: `close(outer_epoll)` → `ep_clear_and_put(outer_ep)` → `__ep_remove(outer_ep, epi1)`
- GDB patches PC at `__ep_remove+0x19c` (after `WRITE_ONCE`, before `hlist_del_rcu`) with `b .`
- Thread B: `close(inner_epoll_1)` → `eventpoll_release` → lockless fast-path (`f_ep==NULL`) → `ep_free(inner_epoll_1_ep)` → kfree to kmalloc-192
- Thread A resumes: `hlist_del_rcu` writes NULL to **freed** `inner_epoll_1_ep->refs.first` (offset 160)

### Natural Preemption Points

| Point | Location | After | Before | Can Enable Race For |
|-------|----------|-------|--------|---------------------|
| **P1** | Line 888 (Loop 1) | `ep_unregister_pollwait(epi_N)` | Loop 2 `ep_remove_safe(epi_N)` | **epi_N** (current epitem) |
| **P2** | Line 903 (Loop 2) | `ep_remove_safe(epi_N)` → `__ep_remove` complete | `ep_unregister_pollwait(epi_{N+1})` | **epi_{N+1}** (next epitem) |

### Multi-Epitem Topology Hypothesis

**Topology**: Outer epoll watches **2+ inner epoll fds** (epi1 → inner_epoll_1, epi2 → inner_epoll_2, ...)

**Race via P1 (Line 888)**:
1. Thread A: `ep_clear_and_put(outer_ep)`, holds `outer_ep->mtx`
2. Loop 1: processes epi1 → `ep_unregister_pollwait(epi1)` → **yields at P1 (line 888)**
3. Thread B: `close(inner_epoll_1)` → `eventpoll_release(inner_epoll_1_file)` → sees `f_ep` still valid → `eventpoll_release_file` → `ep_clear_and_put(inner_epoll_1_ep)` → `ep_free(inner_epoll_1_ep)` → kfree to kmalloc-192
4. Thread A resumes: Loop 2 processes epi1 → `ep_remove_safe(epi1)` → `__ep_remove`:
   - `file = inner_epoll_1_file` (still valid, refcount > 0)
   - `head = file->f_ep` = dangling pointer to freed `inner_epoll_1_ep->refs`
   - `WRITE_ONCE(file->f_ep, NULL)` → writes to file struct (valid)
   - `hlist_del_rcu(&epi1->fllink)` → **UAF WRITE to freed `inner_epoll_1_ep->refs` (offset 160)**

**Race via P2 (Line 903)**:
1. Thread A: completes epi1 in both loops (including `__ep_remove` for epi1)
2. Loop 2: after `ep_remove_safe(epi1)` → **yields at P2 (line 903)**
3. Thread B: `close(inner_epoll_2)` → frees `inner_epoll_2_ep` to kmalloc-192
4. Thread A resumes: Loop 1 processes epi2 → `ep_unregister_pollwait(epi2)` → Loop 2 processes epi2 → `__ep_remove(epi2)` → UAF write on freed `inner_epoll_2_ep->refs`

**Both P1 and P2 are viable natural preemption points** in a multi-epitem topology. P1 targets the current epitem; P2 targets the next epitem.

### Single-Epitem Topology (Prior Experiments)
All prior GDB-forced experiments (EXP-015, 018, 019, 022b, 023b) used **single epitem** (outer epoll watching one inner epoll). In this topology:
- P1: yields after `ep_unregister_pollwait` for the only epitem. Thread B frees inner_epoll. Thread A then does `__ep_remove` → UAF. **Viable.**
- P2: yields after `__ep_remove` for the only epitem. No next epitem exists. **Not viable for UAF.**

The GDB patch at `__ep_remove+0x19c` creates an artificial preemption point **inside** `__ep_remove` (between `WRITE_ONCE` and `hlist_del_rcu`), which does not exist naturally. The natural points P1/P2 are **outside** `__ep_remove`, at loop boundaries in `ep_clear_and_put`.

## Conclusion

**STATIC-HIGH-CONFIDENCE**: The `cond_resched()` at line 903 (and line 888) in `ep_clear_and_put` are **real, reachable preemption points** in a `PREEMPT_VOLUNTARY` kernel. In a **multi-epitem topology** (outer epoll watching 2+ inner epolls), these points can naturally interleave Thread A's epitem processing with Thread B's inner epoll close, enabling the UAF race **without debugger assistance**.

**Next Step**: NAT-001 harness must use **multi-epitem topology** (outer epoll with 2+ epitems watching distinct inner epoll fds) and target the P1/P2 yield windows via CPU pinning, `SCHED_FIFO`, and tight synchronization — not the single-epitem topology used in all prior experiments.

## Evidence Files
- Source: `third_party/linux-6.12.67/fs/eventpoll.c` lines 870-909, 804-859
- Disassembly: `vmlinux` symbols `ep_clear_and_put`, `ep_remove_safe`, `__ep_remove` (this kernel build: commit 7e35917775b8)
- This analysis: `tier2/evidence/NAT-002/NAT-002_RESULTS.md`

## Status
**STATIC-HIGH-CONFIDENCE** — Source and disassembly audit complete. Runtime verification required via NAT-001 statistical test with multi-epitem harness.