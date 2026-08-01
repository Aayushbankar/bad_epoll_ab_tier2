# EXP-019: Controlled Crash PoC (Chain 0)

## Objective
Trigger a controlled kernel panic by spraying msg_msg with an invalid pointer at offset 136 (ep->user), causing `percpu_counter_dec` to dereference garbage.

## Methodology
1. Extended EXP-018 harness (`test_exp019.c`) with crafted msg_msg spray data:
   - Slab offset 96 (ep->lock): 0x00000000 (unlocked)
   - Slab offset 104 (ep->rbr.rb_root.rb_node): 0x0 (NULL)
   - Slab offset 112 (ep->rbr.rb_leftmost): 0x0 (NULL)
   - **Slab offset 136 (ep->user): 0xDEAD000000000000 (invalid kernel pointer)**
   - All other bytes: 0x00
2. GDB script (`exp019_gdb.py`) with same race setup as EXP-018:
   - Break at `__ep_remove+0x19c`, suspend Thread A
   - Break at `ep_free` (hardcoded `0xffff8000802bcbb4`), capture freed address
   - Break at `ep_free+8` (hardcoded `0xffff8000802bcbbc`), dump memory (BEFORE spray)
   - Wait 3s for spray, interrupt, dump memory (AFTER spray)
   - Verify marker at offset 136
   - Restore Thread A instruction, continue
   - Capture crash evidence (backtrace, registers, faulting address)

## Results

### Memory Dump BEFORE Spray (after ep_free, freed inner_epoll)
```
0xffff0000035c49c0:  0x0000000000000000  0x0000000000000000
0xffff0000035c49d0:  0xffff0000035c49d0  0xffff0000035c49d0
0xffff0000035c49e0:  0x0000000000000000  0xffff0000035c49e8
0xffff0000035c49f0:  0xffff0000035c49e8  0x0000000000000000
0xffff0000035c4a00:  0xffff0000035c4a00  0xffff0000035c4a00
0xffff0000035c4a10:  0xffff0000035c4a10  0xffff0000035c4a10
0xffff0000035c4a20:  0xffff0000035c4a80  0x0000000000000000
0xffff0000035c4a30:  0x0000000000000000  0xffffffffffffffff
0xffff0000035c4a40:  0x0000000000000000  0xffff80008152be98  <-- offset 136 (ep->user)
0xffff0000035c4a50:  0xffff000002557480  0x0000000000000001
0xffff0000035c4a60:  0xffff000003550750  0x0000000000000000
0xffff0000035c4a70:  0x0000000000000000  0x0000000000000000
```

### Memory Dump AFTER Spray (3 second wait)
```
0xffff0000035c49c0:  0xffff0000035c4a80  0xffff000003559fc0
0xffff0000035c49d0:  0x0000000000000001  0x0000000000000090
0xffff0000035c49e0:  0x0000000000000000  0xffff000002526718
0xffff0000035c49f0:  0x0000000000000000  0x0000000000000000
0xffff0000035c4a00:  0x0000000000000000  0x0000000000000000
0xffff0000035c4a10:  0x0000000000000000  0x0000000000000000
0xffff0000035c4a20:  0x0000000000000000  0x0000000000000000
0xffff0000035c4a30:  0x0000000000000000  0x0000000000000000
0xffff0000035c4a40:  0x0000000000000000  0xdead000000000000  <-- offset 136 (ep->user) OVERWRITTEN
0xffff0000035c4a50:  0x0000000000000000  0x0000000000000000
0xffff0000035c4a60:  0x0000000000000000  0x0000000000000000
0xffff0000035c4a70:  0x0000000000000000  0x0000000000000000
```

### Marker Verification at Offset 136
```
Marker at 0xffff0000035c4a48 (offset 136): 0xffff0000035c4a48:  0xdead000000000000
```
**Expected:** `0xDEAD000000000000`  
**Actual:** `0xdead000000000000` ✓ **MATCH**

### Thread A Resumption
- Original instruction restored at `__ep_remove+0x19c`
- Execution continued
- **No crash occurred** - hang timeout (25s) triggered
- Thread A completed `__ep_remove` without faulting

## Analysis

### Critical Finding 1: msg_msg Spray Successfully Reclaims Freed Slot ✓
The spray **works**. The freed `inner_epoll` slot at `0xffff0000035c49c0` was fully overwritten by msg_msg allocations. Our crafted marker `0xdead000000000000` landed precisely at offset 136 (`ep->user` field). This confirms EXP-018's reclaim primitive is reliable.

### Critical Finding 2: percpu_counter_dec Does NOT Dereference Freed Eventpoll's user Pointer ✗
**Thread A did not crash** because the `percpu_counter_dec(&ep->user->epoll_watches)` call in `__ep_remove` operates on the **OUTER eventpoll** (`ep` parameter), NOT the freed INNER eventpoll.

**Code path analysis:**
- Thread A: `close(outer_epoll_fd)` → `ep_clear_and_put(outer_ep)` → `__ep_remove(outer_ep, epi_for_inner)`
- In `__ep_remove(outer_ep, epi)`:
  - `ep` = OUTER eventpoll (VALID, not freed)
  - `file = epi->ffd.file` = INNER epoll file
  - `WRITE_ONCE(file->f_ep, NULL)` clears INNER file's f_ep
  - `hlist_del_rcu(&epi->fllink)` → **UAF write to FREED inner_ep at offset 160**
  - `rb_erase_cached(&epi->rbn, &outer_ep->rbr)` → operates on OUTER ep (VALID)
  - `spin_lock_irq(&outer_ep->lock)` → operates on OUTER ep (VALID)
  - `percpu_counter_dec(&outer_ep->user->epoll_watches)` → operates on OUTER ep (VALID)

The ONLY UAF operation on the freed INNER eventpoll is `hlist_del_rcu` at offset 160. The subsequent operations (rb_erase_cached, spin_lock_irq, percpu_counter_dec) all use the OUTER eventpoll which remains valid throughout.

### Offset Mapping Verification
The RUNNER_GUIDE claimed offset 136 = `ep->user` of the FREED eventpoll. Our spray confirmed attacker control at offset 136 of the FREED slot. However, this field is **never read** by Thread A after the reclaim in this code path.

## Conclusion

**FAILED** - The controlled crash was not achieved. The `percpu_counter_dec` arbitrary-decrement primitive **does not exist on the freed eventpoll** in this race setup. Thread A's `__ep_remove` operates on the OUTER eventpoll (valid), not the freed INNER eventpoll.

**Value of this negative result:**
1. **Spray primitive confirmed solid** - msg_msg reliably reclaims freed `struct eventpoll` in `kmalloc-192` with full attacker control at all offsets (48 onward)
2. **Chain 0 hypothesis disproved** - `percpu_counter_dec` is not a UAF primitive on the freed object in the outer-close/inner-close race
3. **Redirects research** - Future chains must target the ONLY actual UAF write: `hlist_del_rcu` at offset 160 (NULL write to freed slot's `epitems_head.first`)

**Next Steps:**
- EXP-020/021: Analyze `rb_erase_cached` behavior on OUTER epoll (valid object, not UAF)
- Investigate if `hlist_del_rcu` NULL write at offset 160 of reclaimed slot is exploitable
- Consider alternative race setups where the freed eventpoll IS the `ep` parameter to `__ep_remove`

This result prevents building Chains 1-3 on a false foundation. The percpu_counter_dec theory requires a different race setup where the freed eventpoll is the one being cleared.