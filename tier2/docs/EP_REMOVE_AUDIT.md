# TASK 2: `__ep_remove` Instruction Audit

This document systematically breaks down the execution of `__ep_remove()` (the teardown function responsible for the UAF corruption window) to identify all writes to the freed `struct epitem` (the `epi` pointer).

**Target Function:** `__ep_remove(struct eventpoll *ep, struct epitem *epi, bool force)`

### 1. `ep_unregister_pollwait(ep, epi)`
- **Action:** Drains wait queues.
- **Modifies:** `epi->pwqlist` (offset 64).
- **Corrupting effect:** Inlined loop reads `pwq = epi->pwqlist` and updates `epi->pwqlist = pwq->next` until it reaches `NULL`. If the UAF race is triggered, this writes `NULL` to offset 64 of the reclaimed memory. (Note: EXP-012 proved this occurs before the actual `__ep_remove` logic during `close(epoll_fd)`, so `pwqlist` is already `NULL` upon entry).

### 2. `spin_lock(&file->f_lock)` & `hlist_del_rcu(&epi->fllink)`
- **Action:** Unlinks the `epitem` from the target file's `f_ep` list.
- **Modifies:** `*epi->fllink.pprev` and `epi->fllink.pprev` (offset 88).
- **Corrupting effect:** 
  1. `*epi->fllink.pprev = epi->fllink.next` (This modifies the previous node's `next` pointer, not `epi` itself).
  2. `epi->fllink.pprev = LIST_POISON2` (Writes `0xdead000000000122` to offset 88 of the reclaimed memory).

### 3. `rb_erase_cached(&epi->rbn, &ep->rbr)`
- **Action:** Unlinks from the eventpoll's red-black tree.
- **Modifies:** The tree structure (parent/children).
- **Corrupting effect:** In the Linux kernel, `rb_erase` does not clear or poison the erased node's pointers (`epi->rbn` at offset 0). The `epi->rbn` pointers remain unmodified (they are stale pointers to the live rbtree), so there is no corrupting memory write directly to `epi` here.

### 4. `list_del_init(&epi->rdllink)`
- **Action:** Unlinks from the eventpoll's ready list.
- **Modifies:** `epi->rdllink.next` (offset 24) and `epi->rdllink.prev` (offset 32).
- **Corrupting effect:** Writes `&epi->rdllink` (the address of the `epitem` + 24) into both offsets 24 and 32. This is the primary UAF write identified in VER-011.

### 5. `wakeup_source_unregister(ep_wakeup_source(epi))`
- **Action:** Unregisters the wakeup source.
- **Modifies:** Internal wakeup source state.
- **Corrupting effect:** Only reads `epi->ws` (offset 96). No writes to `epi` itself.

### 6. `kfree_rcu(epi, rcu)`
- **Action:** Schedules the `epitem` for RCU destruction.
- **Modifies:** `epi->rcu.next` and `epi->rcu.func`.
- **Corrupting effect:** Writes the RCU callback pointer (`kfree`) and next pointer to offset 104 (`epi->rcu`). This happens *as part of the free process* and is standard RCU machinery, but it does overwrite offsets 104 and 112.

### Conclusion of Audit
The *only* memory modifications made to `struct epitem` within `__ep_remove` are:
1. **Offset 24 / 32 (`rdllink`):** Overwritten with self-pointers (`&epi->rdllink`).
2. **Offset 64 (`pwqlist`):** Overwritten with `NULL` (or `pwq->next`).
3. **Offset 88 (`fllink.pprev`):** Overwritten with `LIST_POISON2` (`0xdead...122`).
4. **Offset 104 / 112 (`rcu`):** Overwritten with RCU callback data by `kfree_rcu`.

Because these writes execute sequentially *before* the actual RCU grace period elapses (and before the memory can be reclaimed by an attacker via cross-cache/same-cache), they are not "stale writes to a reclaimed object". Instead, they are the teardown sequence of the object. Any concurrent RCU reader (like `ep_item_poll`) that accesses the object during the grace period will see exactly this state (e.g., `rdllink` pointing to itself, `pwqlist == NULL`).

This confirms that there are no hidden or unexpected writes in `__ep_remove` that an attacker could exploit for arbitrary write or type-confusion primitives during the grace period. The only viable path forward for exploitation remains `ep_item_poll`'s read-side behavior on the unlinked object.
