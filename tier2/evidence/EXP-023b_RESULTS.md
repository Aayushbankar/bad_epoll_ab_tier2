# EXP-023b: percpu_counter_dec Arbitrary Decrement Test (GDB-assisted)

## Objective
Test if `percpu_counter_dec` can be redirected to decrement an attacker-chosen address (modprobe_path) by overwriting `ep->user` pointer in the UAF race.

## Methodology
1. **Harness** (`test_exp023b.c`): Dual msg_msg spray (mtype=0x1337 for reclaim, mtype=0x1338 for fake user_struct)
2. **GDB Script** (`exp023b_gdb.py`):
   - Same outer-close/inner-close race as EXP-022b
   - After spray, GDB writes fake user_struct to memory and overwrites `outer_epoll->user` (offset 136) to point to it
   - Sets breakpoint at `percpu_counter_add_batch` to catch the decrement
   - Checks if modprobe_path[0] changes from '/' (0x2f) to '.' (0x2e)

## Results

### Spray Success ✓
The msg_msg spray successfully reclaims the freed inner_epoll slot:
```
BEFORE spray (offset 136): 0xffff80008152be98 (root_user)
AFTER spray (offset 136):  0xfffffffffffffff8 (marker)
AFTER spray (offset 160):  0xdead000000000000 (marker)
```
mtype=0x1337 message visible at reclaimed slot.

### percpu_counter_add_batch Breakpoint Hit ✓
```
Thread 1 hit Breakpoint 4, percpu_counter_add_batch 
(fbc=0xffff80008152bea0 <root_user+8>, amount=0xffffffffffffffff, batch=0x20)
```
The primitive is reachable and executes.

### Fake user_struct Redirect FAILED ✗
- **GDB wrote fake user_struct** at `inner_epoll_addr + 0x200` with:
  - lock=0, count=0, counters=modprobe_path (0xffff80008161d668)
- **GDB overwrote outer_epoll->user** (offset 136) to `fake_user_struct_addr - 8`
- **But breakpoint shows**: `fbc = 0xffff80008152bea0` = `root_user + 8`
- **modprobe_path unchanged** - still "/sbin/modprobe"

The kernel read `outer_epoll->user` as `root_user` (0xffff80008152be98), not our fake pointer.

## Analysis

### Root Cause: Wrong ep Parameter
In the outer-close/inner-close race:
- Thread A: `close(outer_epoll)` → `ep_clear_and_put(outer_ep)` → `__ep_remove(outer_ep, epi_for_inner)`
- **`ep` parameter = OUTER epoll (VALID, never freed)**
- `percpu_counter_dec(&ep->user->epoll_watches)` operates on **OUTER epoll's user**
- OUTER epoll is NEVER freed → cannot be reclaimed via msg_msg spray
- GDB overwrite of `outer_epoll->user` didn't redirect the decrement (possible cache coherency/timing issue)

### Chain 2 Requires Different Race
The RUNNER_GUIDE's Chain 2 assumes `ep->user` of the **FREED** eventpoll is used. But in our race:
- Freed = INNER epoll (target of `hlist_del_rcu` only)
- `percpu_counter_dec` uses OUTER epoll (valid)

To make Chain 2 work, we need a race where the **freed eventpoll IS the `ep` parameter** to `__ep_remove`. This would require:
- Thread A: `close(inner_epoll)` → `ep_clear_and_put(inner_ep)` → `__ep_remove(inner_ep, epi)` suspended
- Thread B: Somehow frees `inner_ep` before `percpu_counter_dec`
- But `inner_ep` refcount is held by the epoll fd being closed

### Alternative: hlist_del_rcu NULL Write
The ONLY verified UAF write on the freed slot is `hlist_del_rcu` at offset 160 (NULL write with 1 epitem, kernel pointer leak with 2+ epitems). This is the viable primitive.

## Conclusion
**INCONCLUSIVE** - The `percpu_counter_dec` primitive executes but cannot be redirected in the outer-close/inner-close race. The `ep` parameter is the OUTER epoll (valid), not the freed INNER epoll. GDB-assisted overwrite of `outer_epoll->user` failed to redirect the decrement.

**Value of this result:**
1. `percpu_counter_add_batch` is confirmed reachable in the race
2. Chain 2 as designed is NOT viable with current race setup
3. Redirects focus to the ONLY working UAF primitive: `hlist_del_rcu` NULL write / pointer leak at offset 160

**Next Steps:**
- EXP-020/021: Analyze `rb_erase_cached` behavior (operates on OUTER epoll, valid)
- Focus exploitation on `hlist_del_rcu` primitive (offset 160)
- Consider alternative race setups for Chain 2