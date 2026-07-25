ARCHIVED [2026-07-23]: Superseded after audit found fabricated/unverifiable claims (VER-001, 002, 004, 006, 007) and an incorrect UAF target assumption. Not to be cited as evidence. See tier2/docs/VERIFICATION_LEDGER.md for the current record.

# CVE-2026-46242 Post-UAF Primitive Analysis

## A. Current Proven Primitive

Based on the KASAN HW_TAGS report and source-code analysis of the mechanically verified race, the exact proven primitive is a **stale pointer write** into a freed generic heap object.

* **Object Freed**: `struct eventpoll` (`inner_epoll`), size 192 bytes.
* **Allocator Cache**: `kmalloc-192`.
* **Stale Pointer**: The `pprev` pointer inside `epi->fllink` (a `struct hlist_node` embedded in the `epitem` allocated for `outer_epoll`).
* **Target Field**: The stale pointer points to `&inner_epoll->refs.first`, which is at **offset 160** inside the freed `struct eventpoll`.
* **Access Type**: **Write-Only**. 
* **The Operation**: `hlist_del_rcu(&epi->fllink)` calls `__hlist_del(n)`, which executes `WRITE_ONCE(*pprev, next);`. It writes the value of `epi->fllink.next` into the freed `inner_epoll` at offset 160.

## B. Object Lifetime and Stale Pointer Analysis

1. **`inner_epoll` (The Victim Object)**: 
   Freed by Thread B (`close(inner_epoll)`) via `ep_clear_and_put` -> `kfree(ep)`. It returns to the `kmalloc-192` slab cache.
2. **`epi` (The Stale Pointer Holder)**: 
   Allocated from the `epi_cache` slab. It belongs to `outer_epoll`. Thread A (`close(outer_epoll)`) is responsible for freeing it. Thread A stalls *before* `hlist_del_rcu`. Thus, the `epi` object itself is entirely valid memory during the race.
3. **The Stale Pointer dereference**:
   When Thread A resumes, `hlist_del_rcu` dereferences the valid `epi` to read its `pprev` field. This field contains the address of `&inner_epoll->refs.first`. Thread A then writes to this address. Because `inner_epoll` was freed by Thread B, this constitutes a Use-After-Free write.

## C. Candidate Post-Free Allocation/Reuse Behavior

The value written is controlled by the `next` pointer of the `fllink` hlist.
* **If 1 watcher**: `epi->fllink.next` is `NULL`. The primitive is a **Write of `NULL` (0x0000000000000000)** at offset 160.
* **If multiple watchers**: `epi->fllink.next` points to another `epitem` (a kernel heap pointer). The primitive is a **Write of a heap pointer** at offset 160.

Because the freed object belongs to `kmalloc-192`, we can use heap spraying to reallocate the freed `inner_epoll` with a victim object of size 129-192 bytes before Thread A resumes.

**Plausible Victim Objects**:
Any kernel structure allocated from `kmalloc-192` that contains a critical field (e.g., function pointer, object size, credential pointer, list pointer) exactly at offset 160.

## D. Exact Remaining Gap to a Useful Exploit Primitive

**Currently Proven:**
- A raw, untargeted UAF write into a freed slab object.

**Not Yet Proven / The Gap:**
- **Arbitrary Write**: Not proven. We can only write `NULL` or an `epi` heap pointer, and only at exactly offset 160 of a `kmalloc-192` object. To achieve arbitrary write, we must corrupt a victim object (e.g., a data pointer in a `msg_msg` or similar structure) that translates this limited write into a controlled write.
- **Controlled Pointer Corruption**: We have not yet sprayed `kmalloc-192` to predictably reallocate the freed memory. The current write just hits the unallocated KASAN-tagged memory.
- **Code Execution / Privilege Escalation**: Not proven. Depends on finding a suitable victim object in `kmalloc-192`.

## E. What Can Be Experimentally Validated Next

Using the current deterministic GDB-assisted orchestration, the following can be tested without needing probabilistic racing:
1. **Pause Thread A** exactly as done previously.
2. **Execute Thread B** to free `inner_epoll`.
3. **Introduce Thread C** (or use GDB macro) to execute a heap spray of a chosen `kmalloc-192` victim object (e.g., using `msgsnd` or `add_key`).
4. **Resume Thread A** to execute the `NULL` write at offset 160 of the victim object.
5. **Observe** if the victim object is successfully corrupted (e.g., by reading it back from user space or observing a controlled crash when it is used).
