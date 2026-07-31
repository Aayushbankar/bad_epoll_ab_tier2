# EXP-011: Type-Confusion Pivot Viability

## Objective
Determine if reclaiming the freed `struct file` with a different legitimate file type (e.g., `timerfd`, `signalfd`, `eventfd`) produces a usable primitive when the stale `epitem` later interacts with it via `ep_item_poll`.

## Candidate Analysis (Static Source Review)
We surveyed candidate file types that share the `filp` cache:
1. **`timerfd`**: `timerfd_poll` reads `ctx->ticks`.
2. **`eventfd`**: `eventfd_poll` reads `ctx->count`.
3. **`signalfd`**: `signalfd_poll` reads signal queues.

In all cases, if `ep_item_poll` were to call `file->f_op->poll` on these files, it would safely execute their respective poll routines. The VFS layer enforces type safety via dynamic `f_op` dispatch. The poll functions do not perform unsafe writes or read attacker-controlled pointers in a corruptible way; they merely return standard `EPOLLIN`/`EPOLLOUT` flags based on their internal state. Thus, any such confusion would just "fail safely."

## Reachability Analysis (The Mutex Barrier)
Further analysis of `fs/eventpoll.c` reveals a more fundamental obstacle: **the type confusion is completely unreachable.**

To trigger the `struct file` UAF, an attacker must race `__ep_remove` (Thread A) against `eventpoll_release_file` (Thread B). 
1. Thread A must execute `__ep_remove` (via `EPOLL_CTL_DEL` or `close(outer)`) and set `file->f_ep = NULL`.
2. Thread A is preempted.
3. Thread B executes `close(inner)` and skips cleanup because `file->f_ep` is NULL. Thread B frees the inner `struct file`.
4. Thread C reclaims the `struct file` with a `timerfd`.

At this point, the `epitem` holds a stale pointer to the reclaimed `timerfd` file. However, to trigger `ep_item_poll` on this stale `epitem`, another thread (Thread D) must call `epoll_wait` or `EPOLL_CTL_MOD` on the outer `epoll`.

**The Blocking Issue:**
Both `epoll_wait` (via `ep_send_events`) and `EPOLL_CTL_MOD` require acquiring `ep->mtx`. 
Thread A, which is currently suspended inside `__ep_remove`, **already holds `ep->mtx`** (acquired by `do_epoll_ctl` or `ep_clear_and_put`).

Thread D will block indefinitely on `ep->mtx`. When Thread A resumes, it will finish `__ep_remove` by:
1. Removing the `epitem` from the ready list (`list_del_init(&epi->rdllink)`).
2. Removing the `epitem` from the RB tree (`rb_erase_cached(&epi->rbn, &ep->rbr)`).
3. Scheduling the `epitem` itself for RCU destruction (`call_rcu(&epi->rcu, epi_rcu_free)`).
4. Releasing `ep->mtx`.

By the time Thread D acquires `ep->mtx`, the stale `epitem` has been completely unlinked and destroyed. Thread D cannot find it in the ready list or the RB tree. Therefore, **`ep_item_poll` can never be executed on the stale `epitem`**.

## Unexplained Artifacts Handled
- **Artifact:** Unexplained `__fput` on `0xffffff8003f39300` during `do_epoll_wait` in earlier logs.
- **Explanation:** This was almost certainly the harness process exiting and VFS closing a sprayed timerfd.
- **Log Null Bytes:** The null bytes and duplicate "SETTING BREAKPOINTS" seen in raw logs were artifacts of the Python GDB harness script's output buffer handling upon test timeout/retry, not a silent failure masking true logic.

## Conclusion
The `struct file` UAF cannot be exploited via type confusion through `ep_item_poll` due to the `ep->mtx` barrier. Note: A previously proposed theory (EVO-007) that the primitive could rely on cross-cache corruption of `struct eventpoll` in `kmalloc-192` was definitively disproved by VER-021. The vulnerability must be exploited through another path (such as `epitem` same-cache reclaim as seen in VER-016 analysis, or EXP-012 exploration).
