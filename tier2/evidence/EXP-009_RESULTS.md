# EXP-009: File Object UAF Primitive Verification

## Objective
Verify that `struct file` UAF can be successfully reclaimed using `open()`. 
Provide register-level proof that the pointer obtained by racing epoll `close` is actually reused during spray.

## Results
- Run command: `./tier2/scripts/run_exp009.sh`
- Captured stale pointer in Thread 1: `0xffffff800233de00`
- Hit `__fput` in Thread 2 with matching pointer `0xffffff800233de00`.
- Verified via `[DEBUG] file_free_rcu called with x0=0xffffff800233de00`.
- Found `[!!!] BINGO: RECLAIM SUCCESSFUL! struct file allocated at 0xffffff800233de00 overlaps stale file_ptr!` when `open()` spraying in Thread 3. 
- Disassembly/breakpoint verification confirmed `$x0` = `0xffffff800233de00` upon returning from `kmem_cache_alloc`.

## Conclusion
PASSED. We have definitively established that a `struct file` target pointer can be extracted during the `epoll` race condition, subsequently freed via `file_free_rcu` triggered by inner `epoll` closure, and then exactly reclaimed by a new `struct file` allocation using `open()`.

## Next Steps
EXP-010: Proceed to UAF exploitation using `timerfd` or `signalfd` which contain function pointers that can be controlled to achieve code execution.
