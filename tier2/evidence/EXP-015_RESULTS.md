# EXP-015: Struct File UAF Exploitability & kmalloc-192 Revival

## 1. Goal
Evaluate whether the `struct file` UAF (proved in EXP-009) can be turned into an exploit primitive using same-cache reclaim, given that `pipe_buffer` cross-cache reclaim is blocked (EXP-010). If it is a dead end, re-evaluate the previously "disproved" `struct eventpoll` (`kmalloc-192`) UAF theory.

## 2. Analysis of the `struct file` UAF
The `struct file` UAF occurs when Thread A (`close(outer_epoll)`) clears `file_target->f_ep`, allowing Thread B (`close(inner_epoll)`) to bypass `eventpoll_release_file` and immediately call `file_free(file_target)`. 
If an attacker reclaims the freed `struct file` via the `filp` cache (e.g., by opening a new file), Thread A resumes and executes its remaining instructions on the newly reallocated `struct file`.

### Thread A's Stale Accesses
Disassembly of `__ep_remove` confirms that Thread A makes exactly two memory accesses to the target `struct file` after it has been freed and reallocated:
1. **Read at offset 16 (`file->f_op`)**: Via `is_file_epoll(file)`.
2. **Write of 0 at offset 8 (`file->f_lock`)**: Via `spin_unlock(&file->f_lock)`.

### Same-Cache (filp) Primitive Evaluation
The `filp_cachep` slab is created with `SLAB_TYPESAFE_BY_RCU`, which prevents merging. A global code audit confirms that `filp_cachep` is exclusively used by `fs/file_table.c` to allocate `struct file`.
Therefore, the attacker **must** reclaim the object as a `struct file`—no other kernel object (like `timerfd_ctx` or generic `kmalloc` objects) can occupy this slot.

Because the object is always a `struct file`:
- The kernel will legitimately populate `f_op` at offset 16 with a valid file operations pointer (e.g., `&timerfd_fops`). Thread A will read it, evaluate it against `&eventpoll_fops` (resulting in `false`), and proceed safely.
- The kernel will legitimately initialize the new file's `f_lock` (at offset 8) to `0` (unlocked). When Thread A executes `spin_unlock`, it writes `0` over a `0`. This is a harmless no-op.

**Conclusion:** The `struct file` UAF is a **structural dead end**. There is no type confusion or exploitable overlap possible because the `filp` cache is rigidly homogeneous.

## 3. The `kmalloc-192` UAF Revival (Flaw in EXP-013)
Given the `struct file` UAF is a dead end, we must revisit the `kmalloc-192` UAF on `struct eventpoll`. 
In VER-021 (EXP-013), this theory was marked "DISPROVED" because a hardware watchpoint trace showed `hlist_del_rcu` executing *before* the `eventpoll` struct was freed. 

**However, EXP-013 was a single-threaded trace.** It fundamentally failed to model the multi-threaded race condition that triggers the vulnerability. 

### The True Multi-Threaded Race
The race occurs between Thread A closing the `outer_epoll` and Thread B closing the `inner_epoll` (target file):
1. **Thread A** is in `__ep_remove`. It holds `outer_epoll->mtx` and `target_file->f_lock`.
2. **Thread A** executes `WRITE_ONCE(target_file->f_ep, NULL)`.
3. **Thread A is preempted.**
4. **Thread B** calls `__fput(target_file)`. It checks `target_file->f_ep` and sees `NULL`.
5. Because it sees `NULL`, **Thread B locklessly bypasses `eventpoll_release_file`**. Crucially, this means Thread B **does not attempt to acquire `target_file->f_lock` or `outer_epoll->mtx`**.
6. **Thread B** proceeds to `ep_eventpoll_release`, acquires `inner_epoll->mtx` (which is uncontended), and calls `ep_free(inner_epoll)`. This **FREES** the `struct eventpoll` (`kmalloc-192`) belonging to the inner epoll.
7. **Thread A wakes up.** It executes `hlist_del_rcu(&epi->fllink)`.

### The Stale Write
In older kernels (like the Tier 2 6.1.67 kernel), `file->f_ep` for an epoll file points to `&ep->refs` inside the `struct eventpoll`.
When `hlist_del_rcu` removes `epi` from the list, it writes `NULL` (0) to the list head pointer (`ep->refs.first`).
A check of `struct eventpoll` in GDB confirms `sizeof(struct eventpoll) == 176` (so it lands in `kmalloc-192`) and `refs` is at **offset 160**.

**Conclusion:** Thread A writes 8 bytes of `0` at offset 160 of the **FREED** `kmalloc-192` object. This confirms that the original CVE documentation in `VULNERABILITY.md` is 100% correct, and my earlier disproof in VER-021 was methodologically flawed.
