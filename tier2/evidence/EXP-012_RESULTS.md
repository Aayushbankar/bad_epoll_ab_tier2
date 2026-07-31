# EXP-012: Structural Isolation of Corrupted `epitem` during RCU Grace Period

## Objective
Investigate whether the corrupted `epitem` (specifically the `rdllink` overwritten at offsets 24/32 by `list_del_init`) is reachable via any path other than `ep_item_poll` before RCU actually frees the memory. This includes analyzing concurrency with `EPOLL_CTL_ADD/MOD/DEL` and RCU read-side critical sections.

## Methodology
Static analysis of `fs/eventpoll.c` (Kernel 6.1.67) to trace all access paths to `struct epitem` and its `rdllink` field during the race window inside `__ep_remove`.

## Analysis

The race in question occurs in `__ep_remove(struct eventpoll *ep, struct epitem *epi, bool force)`. When executed, the function performs the following unlinking operations in order:

1. **`ep_unregister_pollwait(ep, epi);`**
   - Synchronously removes the wait queue entries (`pwqlist`). 
   - Wait-queue lock synchronization ensures any running `ep_poll_callback` finishes before this returns, and no future callbacks can fire.

2. **`spin_lock(&file->f_lock); ... hlist_del_rcu(&epi->fllink); spin_unlock(&file->f_lock);`**
   - Removes the `epitem` from the monitored file's hook list. 
   - Concurrent RCU readers (`reverse_path_check_proc`, `ep_get_upwards_depth_proc`) can still traverse past this node during the grace period.

3. **`rb_erase_cached(&epi->rbn, &ep->rbr);`**
   - Removes the `epitem` from the outer epoll's Red-Black tree.

4. **`spin_lock_irq(&ep->lock); ... list_del_init(&epi->rdllink); spin_unlock_irq(&ep->lock);`**
   - Removes the `epitem` from the ready list. **This is the instruction that corrupts offsets 24/32.**

5. **`kfree_rcu(epi, rcu);`**
   - Schedules the memory for freeing.

### Evaluation of Concurrent Access Paths

Once `list_del_init(&epi->rdllink)` executes (corrupting the pointers), we evaluate if any concurrent thread can reach this corrupted field before the RCU grace period ends:

#### 1. `EPOLL_CTL_MOD` and `EPOLL_CTL_DEL`
Both operations rely on `ep_find()` to locate the target `epitem`. `ep_find()` searches the `ep->rbr` Red-Black tree. Because `rb_erase_cached` executes **before** the `rdllink` corruption, `ep_find()` will return `NULL`. Thus, `EPOLL_CTL_MOD/DEL` cannot reach the corrupted object.

#### 2. `EPOLL_CTL_ADD`
If `ep_find()` returns `NULL`, `EPOLL_CTL_ADD` allocates a brand-new `epitem`. It has no mechanism to find or reuse the corrupted `epitem` still in the grace period.

#### 3. `ep_poll_callback` (Wait-Queue Wakeups)
The poll callback reads and modifies `epi->rdllink`. However, as established, `ep_unregister_pollwait()` removes the callback hook synchronously **before** the `rdllink` corruption occurs.

#### 4. RCU Read-Side Paths
The only data structure holding a valid path to the `epitem` during the grace period is the `fllink` list (traversable by RCU readers that were already iterating the list when `hlist_del_rcu` was called).
- **`reverse_path_check_proc`** and **`ep_get_upwards_depth_proc`** iterate over `fllink` inside an `rcu_read_lock()`.
- If an RCU reader traverses the freed `epitem`, it accesses `epi->ep` (offset 72) and `epi->ep->refs`. 
- **Crucially**, these RCU readers do **not** access `epi->rdllink` (offsets 24/32). Since the memory is physically intact (prevented from being reallocated by the RCU lock) and `epi->ep` is uncorrupted, the traversal is perfectly safe and cannot be exploited.

#### 5. `ep_send_events` (via `epoll_wait`)
Transfers events from the ready list to userspace. It requires `ep->mtx` to execute. Since `__ep_remove` also holds `ep->mtx` during the race, `ep_send_events` is strictly serialized and cannot concurrently access the `rdllink` during the grace period.

## Conclusion
**STATIC-HIGH-CONFIDENCE - STRUCTURAL DEAD-END.** 

The corrupted `epitem` is completely isolated during the RCU grace period. The `rdllink` field is corrupted by `list_del_init`, but every mechanism that could theoretically read or write to `rdllink` relies on locating the `epitem` via data structures (`rbr`, `pwqlist`, `rdllist`) from which the `epitem` has already been synchronously unlinked. The only paths that can reach the `epitem` during the grace period (RCU traversals) do not access the corrupted offsets.

This confirms that the memory corruption is contained and unreachable before the object is physically freed. Pending a lightweight live-trace of the `ep_unregister_pollwait` drain behavior to upgrade to fully VERIFIED.

### Part 2: Live Trace Verification (EXP-012b)

Following the project's rigorous standards, a GDB trace was executed to confirm the ordering of `remove_wait_queue()` relative to the `list_del_init()` corruption point, and to strictly verify the identity of the `epitem` across both teardown phases.

**Trace Target:** `ep_clear_and_put` (invoked during `close(epoll_fd)`)
**Log:** `tier2/evidence/EXP-012_raw_gdb.log`

The trace captured the execution of a single-threaded closure (which calls `ep_clear_and_put()`), specifically tracking the address of the `epi` being drained vs. the one being corrupted:

```
[*] BREAKPOINT HIT: remove_wait_queue
[*]     wq_entry = -0xfffffcb47a30
[*]     pwq (wq_entry - 16) = -0xfffffcb47a40
[*]     pwq->base (epi) = 0xffff0000034a9880
[*] BREAKPOINT HIT: __ep_remove (entry)
[*]     epi = 0xffff0000034a9880
[*]     IDENTITY MATCH: The epi being removed is the EXACT SAME one drained in remove_wait_queue!
```

**Observation:**
The trace definitively proves that `remove_wait_queue` (which synchronizes with and drains `ep_poll_callback`) executes in a completely separate loop BEFORE `__ep_remove` is even invoked on the exact same `epitem`. 

**Concurrency Caveat:**
This live trace captures a single `close()` execution on one thread. It verifies *instruction order*, not concurrent safety. The argument that this sequence is safe from concurrent `ep_poll_callback` wakeups relies on the structural/static facts (that `ep->mtx` and `whead->lock` correctly serialize access during this teardown). Thus, the instruction order is empirically confirmed, but the concurrency safety argument remains structural.

### Final Conclusion
The instruction ordering is verified as fact: the wait queues are deterministically destroyed before the corruption occurs. Since `ep_poll_callback` requires an active wait queue to reach the `epitem`, the corrupted `rdllink` pointers cannot be reached via this path.

**Status:** Upgrade VER-022 to VERIFIED, with the caveat that the concurrency safety remains structurally deduced. The `ep_item_poll` path (RCU traversal) remains the *only* viable primitive for triggering the UAF.
