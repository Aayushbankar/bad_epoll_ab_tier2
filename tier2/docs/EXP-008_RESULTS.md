# EXP-008 Results

**Objective:**
Determine if the list_del_init(&epi->rdllink) corruption occurs on stale freed memory BEFORE the new epitem spray replaces it, or if it can occur on the NEWLY reclaimed live epitem. Then assess the exploitability.

**Methodology:**
1. Modified `test_epoll_spray.c` to `test_exp008_timing.c` where an event is triggered to put the epitem on the `rdllist`.
2. Created a GDB script (`gdb_exp008_timing.py`) breaking at:
    * `__list_del_entry` (inside `list_del_init` via `__list_del_entry_valid` inline) inside `__ep_remove`.
    * `call_rcu` inside `__ep_remove` to catch the free address.
    * `kmem_cache_alloc` return in `do_epoll_ctl` to catch the reclaim address.
3. Observed the hit order and addresses.

**Findings:**
1. **Timing (Evidence 1 - `tier2/evidence/EXP-008_raw_gdb.log`):**
    * Hit Breakpoint 1 (`list_del_init`): `0xffffff8004804800`
    * Hit Breakpoint 2 (`call_rcu` / free): `0xffffff8004804800`
    * Hit Breakpoint 3 (`kmem_cache_zalloc` return / reclaim): `0xffffff8004804800` on the 2nd allocation.
    
    **Conclusion:** The `list_del_init` corrupting write ALWAYS occurs **BEFORE** the `call_rcu` which frees the object. The reclaim (`kmem_cache_alloc`) happens *after* the free. Therefore, the corruption writes `LIST_POISON1` and `LIST_POISON2` into offsets 24 and 32 of the object *while it is still the original victim epitem being torn down*.
    The new epitem allocated via spray during reclaim will unavoidably overwrite these poisoned fields, not necessarily because of allocation-flag zeroing (`__GFP_ZERO`), but because every legitimate `epitem` setup must call `INIT_LIST_HEAD` on `rdllink`. The disassembly of the spray allocation (see `do_epoll_ctl` at offsets +0x18/+0x20 in the log) executes `add x0, x0, #0x18` followed by `str x0, [x24, #0x18]` and `str x0, [x24, #0x20]`. This stores the address of the `rdllink` field into itself (`next = prev = &self`). A `list_head` can never be validly left in a non-self-pointing or zeroed state upon initialization. The corruption does NOT survive on the live reclaimed object.

2. **Primitive Assessment:**
    * If the corrupting write landed on the newly reclaimed object, we would have a corrupted list linkage on a live object. 
    * Since the corrupting write lands on the memory *before* it is freed, and the memory is then freed via `call_rcu` and eventually reallocated, the corrupting write (`LIST_POISON` values) is completely and unavoidably overwritten by `INIT_LIST_HEAD` when the new `epitem` is initialized.
    * The write to `LIST_POISON1/2` at offsets 24/32 has no lingering effect on the memory slot once it's reclaimed by a new `epitem`. This makes the dead-end conclusion actually stronger and more structurally certain: there is no allocation-flag variant or kernel build config that would avoid this overwrite, as `INIT_LIST_HEAD` is structurally required for a valid list node.

**Conclusion:**
This same-cache `epitem`-into-`epitem` reclaim strategy is a **DEAD END** for this specific corruption (`list_del_init`). The corruption happens before the free, meaning we only corrupt the object being torn down. When we reclaim the slot with a new `epitem`, the new `epitem` is perfectly valid. We gain no primitive from this.

We need a genuinely different reclaim strategy where the *old* `epitem`'s fields are somehow interpreted by the *new* object, or where a different object type can overlap, or we must look for a different corruption vector (e.g., does the UAF read of `struct file` offer anything?).

**Status:** PASSED (Successfully identified dead end).
