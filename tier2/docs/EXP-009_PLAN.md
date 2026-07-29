# EXP-009 Plan

**Objective:**
Investigate the struct file UAF. Specifically, identify if `epi->ffd.file` is accessed after the underlying file's refcount could have dropped to zero, and attempt pipe_buffer cross-cache reclaim of the file struct slot.

**Steps:**
1. **Source Code Analysis:**
   - In `__ep_remove`, we have:
     ```c
     static bool __ep_remove(struct eventpoll *ep, struct epitem *epi, bool force)
     {
         struct file *file = epi->ffd.file; // READ 1
         // ...
         spin_lock(&file->f_lock); // ACCESS 1
         if (epi->dying && !force) {
             spin_unlock(&file->f_lock); // ACCESS 2
             return false;
         }
         // ...
         head = file->f_ep; // ACCESS 3
         // ...
         spin_unlock(&file->f_lock); // ACCESS 4
         // ...
     }
     ```
   - `__ep_remove` can be called from `ep_remove_safe` during `ep_clear_and_put` (which is called by `ep_eventpoll_release` when closing an epoll instance).
   - If `outer_epoll` is closed, it walks its tree and calls `__ep_remove` on each `epi`.
   - If the `epi` represents a watch on `inner_epoll`, then `epi->ffd.file` is the `struct file` for `inner_epoll`.
   - The UAF relies on closing `inner_epoll` concurrently, dropping its file refcount to zero and freeing the `struct file`.
   - But notice that `ep_eventpoll_release` is ONLY called when the refcount of the epoll file drops to zero.
   - The key is that closing `inner_epoll` drops its `f_count` to zero, triggering its `f_op->release` (which is `ep_eventpoll_release`), which frees the file. BUT the `outer_epoll` still has an `epi` that points to `inner_epoll`'s `struct file` via `epi->ffd.file`. This is because epoll does not hold a reference count on the target file!
   - Wait, `eventpoll_release_file` (called from `__fput` of `inner_epoll`) iterates through `inner_epoll->f_ep` and removes the `epi` from `outer_epoll`. 
   - BUT, if `outer_epoll` is ALSO being closed concurrently, we can have a race.

2. **GDB Trace Plan:**
   - Write a reproducer that closes `outer_epoll` and `inner_epoll` concurrently.
   - Set a breakpoint on `kmem_cache_free` (or `kfree_rcu` if `struct file` is RCU delayed) for the `struct file`.
   - Note: `struct file` is freed via `file_free()` which does `kmem_cache_free(filp_cachep, f)`. It's SLAB_TYPESAFE_BY_RCU, not delayed via `call_rcu` normally, but we need to check if we can reclaim it immediately.
   - Oh, `file_free` directly calls `kmem_cache_free(filp_cachep, f)`. So it goes back to the slab freelist immediately.
   - But because of `SLAB_TYPESAFE_BY_RCU`, the pages are delayed before going to the page allocator, but individual objects CAN be re-allocated immediately from the slab freelist by another `alloc_empty_file`!
   - Wait, the user prompt suggests: "attempt pipe_buffer cross-cache reclaim of the file struct slot."
   - Ah! `filp_cachep` in this kernel might NOT have `SLAB_TYPESAFE_BY_RCU`? Or wait, if we use `pipe_buffer`, `pipe_buffer` is allocated via `kmalloc`. Is `filp_cachep` a dedicated cache or kmalloc?
   - Let's check `files_init()` in `fs/file_table.c`.
