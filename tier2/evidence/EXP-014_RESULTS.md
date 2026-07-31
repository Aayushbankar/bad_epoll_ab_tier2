# EXP-014: `eventpoll_release_file` Thread B Race Analysis (REVISED)

**Objective**: Resolve contradictions regarding the lockless CVE fast path, confirm register liveness around `mutex_lock` via disassembly, and assess Thread B's execution path.

## 1. Resolution of the Locking Contradiction (User Point 2)
My previous narrative claimed that `eventpoll_release_file` acquires `epmutex` (the global mutex). This was factually incorrect and based on a mischaracterization of older kernel code. In this specific kernel tree (6.12.67), the execution works as follows:

- **Thread A (`__ep_remove`)**: Acquires `file->f_lock` (inner file), sets `file->f_ep = NULL`, unlinks the `epitem`, and drops `f_lock`. It does **not** hold `epmutex`.
- **Thread B (`eventpoll_release`)**: Reaches the inline function `eventpoll_release()` from `__fput`. It executes the lockless fast path:
  ```c
  if (likely(!READ_ONCE(file->f_ep)))
      return;
  ```
- **The Genuine Race**: The CVE exploits this exact lockless check. If Thread B's `READ_ONCE` occurs immediately after Thread A's `WRITE_ONCE(file->f_ep, NULL)`, Thread B **skips** `eventpoll_release_file` entirely and proceeds to free the inner `struct file`. Meanwhile, Thread A has just dropped `file->f_lock` but may still be executing cleanup tasks, meaning Thread A is left with a dangling pointer to a freed `struct file`. This confirms the CVE mechanism and proves the UAF target is indeed `struct file` (as explored in EXP-009/EXP-010), not `struct epitem` via `eventpoll_release_file`.

## 2. Disassembly and Register Liveness (User Point 3)
Although the `mutex_lock` path in `eventpoll_release_file` is not the true UAF primitive (due to the `epi->dying` flag and `f_lock` serialization preventing the double-execution race), we obtained the disassembly to confirm compiler behavior around the blocking call:

```assembly
ffff8000802bdbbc:  aa1303e0   mov  x0, x19                  // x0 = ep (from x19)
ffff8000802bdbc0:  9428341f   bl   ffff800080ccac3c <mutex_lock>  // block point
ffff8000802bdbc4:  aa1403e1   mov  x1, x20                  // x1 = epi (from x20)
ffff8000802bdbc8:  52800022   mov  w2, #0x1                 // force = true
ffff8000802bdbcc:  aa1303e0   mov  x0, x19                  // x0 = ep (from x19)
ffff8000802bdbd0:  97fffb15   bl   ffff8000802bc824 <__ep_remove> // wake point call
```
**Conclusion from Disassembly:**
The compiler **keeps the original values live in registers** (`x19` for `ep`, `x20` for `epi`). They are **not** reloaded from memory after `mutex_lock` returns. If a UAF were possible here, the Thread B would definitively pass the original dangling pointers directly to `__ep_remove`.

## Conclusion
The previously claimed `eventpoll_release_file` UAF is structurally impossible in this kernel version due to strict `file->f_lock` and `epi->dying` serialization. The actual UAF race strictly follows the original CVE mechanism: Thread B takes the lockless fast path, bypassing `eventpoll_release_file` entirely, and freeing the `struct file` out from under Thread A.

**Status**: STATIC-HIGH-CONFIDENCE (Narrative corrected. Awaiting further live trace if required).
