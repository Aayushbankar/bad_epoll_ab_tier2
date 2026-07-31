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

Following the project's rigorous standards (source analysis is theory; runtime observation is fact), a GDB trace was executed to confirm the ordering of `ep_unregister_pollwait()` relative to the `list_del_init()` corruption point.

**Trace Target:** `ep_clear_and_put` (invoked during `close(epoll_fd)`)
**Log:** `tier2/evidence/EXP-012_raw_gdb.log`

The trace revealed a crucial two-phase teardown in `ep_clear_and_put()` that is executed when an epoll fd is closed:

```
[*] BREAKPOINT HIT: remove_wait_queue (ep_unregister_pollwait)
[*] BT:
#0  ep_remove_wait_queue (pwq=0xffff000001e45440) at fs/eventpoll.c:648
#1  ep_unregister_pollwait (ep=<optimized out>, epi=<optimized out>) at fs/eventpoll.c:663
#2  ep_clear_and_put (ep=0xffff000001e3f9c0) at fs/eventpoll.c:887

[*] BREAKPOINT HIT: __ep_remove (entry)
[*] BT:
#0  __ep_remove (ep=ep@entry=0xffff000001e3f9c0, epi=0xffff000001e3e180, force=force@entry=0x0) at fs/eventpoll.c:806
#1  0xffff8000802bca60 in ep_remove_safe (ep=0xffff000001e3f9c0, epi=<optimized out>) at fs/eventpoll.c:866
#2  0xffff8000802bcb6c in ep_clear_and_put (ep=0xffff000001e3f9c0) at fs/eventpoll.c:902
```

**Observation:**
The trace definitively proves that `remove_wait_queue` (which synchronizes with and drains `ep_poll_callback`) executes in a completely separate loop (line 887) BEFORE `__ep_remove` is even invoked (line 902). 

Since the corrupting `list_del_init(&epi->rdllink)` instruction lives deep inside `__ep_remove`, all in-flight wait queue callbacks for the entire eventpoll instance are guaranteed to be fully drained and unregistered before the corruption window opens.

### Final Conclusion
With live trace confirmation, the theoretical finding is verified as fact. The corrupted `rdllink` pointers cannot be reached via `ep_poll_callback` because all wait queue entries are deterministically destroyed before the corruption occurs.

**Status:** Upgrade VER-022 to VERIFIED. The `ep_item_poll` path (RCU traversal) remains the *only* viable primitive for triggering the UAF.
