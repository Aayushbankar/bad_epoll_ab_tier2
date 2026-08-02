# NAT-002 CORRECTION: cond_resched() is a No-Op in 2-CPU Pinned PREEMPT_VOLUNTARY Setup

## Executive Summary
**The NAT-002 conclusion that `cond_resched()` at lines 888 and 903 creates meaningful preemption windows for the UAF race is INCORRECT for the 2-CPU pinned topology used in NAT-001.**

In a `PREEMPT_VOLUNTARY` kernel (this build: `CONFIG_PREEMPT_VOLUNTARY=y`, `CONFIG_PREEMPT_DYNAMIC=y`) with Thread A pinned to CPU 0 and Thread B pinned to CPU 1 on an otherwise idle system:
- `cond_resched()` / `dynamic_cond_resched()` at lines 888 and 903 **does not yield**
- It returns 0 immediately because `TIF_NEED_RESCHED` is never set on CPU 0
- Thread B on CPU 1 cannot affect CPU 0's scheduling decisions

## Mechanism Analysis

### cond_resched() Implementation Path
```
cond_resched() 
  → dynamic_cond_resched() [static_call, checks sk_dynamic_cond_resched]
    → __cond_resched() [if static key enabled]
      → should_resched(0)
        → preempt_count() == 0 && tif_need_resched()
          → arch_test_bit(TIF_NEED_RESCHED, current_thread_info()->flags)
```

### What Sets TIF_NEED_RESCHED?
In `PREEMPT_VOLUNTARY`:
1. **Timer tick (sched_tick)** → `task_tick_fair()` → `entity_tick(queued=0)` → **does NOT call `resched_curr()`** (only updates runtime stats)
2. **HRTick (queued=1)** → `entity_tick(queued=1)` → calls `resched_curr()` → sets `TIF_NEED_RESCHED` + sends IPI if needed
3. **Wakeups on same CPU** → `resched_curr(rq)` → sets `TIF_NEED_RESCHED`
4. **Explicit `set_tsk_need_resched()`** from other kernel paths

### In Our 2-CPU Pinned Topology:
- Thread A on CPU 0: only runnable task on CPU 0
- Thread B on CPU 1: only runnable task on CPU 1
- Timer tick fires on both CPUs, but with `queued=0` → no `resched_curr()` call
- No wakeups on CPU 0 (Thread B doesn't wake anything on CPU 0)
- No cross-CPU IPIs for scheduling (Thread B's `close()` → `ep_free()` doesn't trigger scheduler IPIs)
- **Result: TIF_NEED_RESCHED on CPU 0 is ALWAYS 0**

### dynamic_cond_resched() Static Key
- `CONFIG_PREEMPT_DYNAMIC=y` with `PREEMPT_VOLUNTARY` default
- `sk_dynamic_cond_resched` = `DEFINE_STATIC_KEY_FALSE` → starts FALSE
- Unless explicitly switched via `/sys/kernel/debug/sched/preempt_dynamic`, remains FALSE
- `dynamic_cond_resched()` returns 0 immediately without calling `__cond_resched()`

**Either way: NO YIELD OCCURS.**

## Disassembly Confirmation
```
ffff8000802bcb30:  bl  ffff800080cc8248 <dynamic_cond_resched>  // Line 888
ffff8000802bcb6c:  bl  ffff800080cc8248 <dynamic_cond_resched>  // Line 903
```

## Impact on NAT-002 Conclusion

| Preemption Point | Prior Claim | Corrected Reality |
|-----------------|-------------|-------------------|
| P1 (line 888) | Thread A yields after `ep_unregister_pollwait(epi_N)` | **No yield** - `cond_resched()` returns 0 |
| P2 (line 903) | Thread A yields after `__ep_remove(epi_N)` | **No yield** - `cond_resched()` returns 0 |

**The multi-epitem topology's value for natural race triggering is NOT from scheduling yields at cond_resched().**

## What Multi-Epitem Topology Actually Provides
Pure **instruction count timing window**:
- Thread A processes epitem 1: `__ep_remove` (~200-500 cycles) + loop overhead (~50 cycles) = ~250-550 cycles
- During this window, Thread B on CPU 1 can complete: `close(inner_epoll_2)` → `eventpoll_release` → lockless path → `ep_free` → `kfree`
- Window: ~125-275 ns on 2GHz ARM64
- **This is a race window, but NOT a scheduling yield window**

## Required NAT-001 Design Pivot

### Option 1: Pure Statistical Race (Current Appendix A Harness)
- 10,000+ iterations per boot
- Relies on nanosecond-scale timing variance
- Hit rate likely << 10⁻⁴ (may be 0 in 10k iterations)
- **Not recommended as primary approach**

### Option 2: Tier 1 Timing-Widening Techniques (RECOMMENDED)
Proven techniques from prior research that manipulate REAL relative timing between CPU-pinned threads:

1. **False-Sharing Cache-Line Bouncing (Third Thread)**
   - Thread C hammers a cache line shared with Thread A's hot path
   - Causes non-deterministic L1/L2 cache misses, memory bus contention
   - Widens Thread A's execution time variance by 10-1000x

2. **Memory Pressure / Slab Contention**
   - Pre-fill kmalloc-192 with allocations
   - Thread A's `kfree_rcu` / Thread B's `kmalloc` contend on slab locks
   - Adds microsecond-scale variance

3. **Timer Interrupt Storms**
   - Even in PREEMPT_VOLUNTARY, timer interrupts add interrupt latency variance
   - Can be amplified via `timerfd` or `setitimer` storms

4. **IPI Storms**
   - Cross-CPU interrupts from Thread C to Thread A's CPU
   - Forces Thread A through interrupt handler, adding variance

## Updated VER-035 Status
**CORRECTED** (was VERIFIED → now STATIC-HIGH-CONFIDENCE with mechanism caveat):
- Source audit of cond_resched locations: **CONFIRMED** (lines 888, 903 exist)
- Disassembly confirmation: **CONFIRMED** (calls to dynamic_cond_resched)
- **BUT**: Mechanism analysis shows **NO YIELD in 2-CPU pinned PREEMPT_VOLUNTARY**
- Multi-epitem topology value: **INSTRUCTION COUNT TIMING WINDOW ONLY**

## Next Steps
1. **Retract VER-035's claim that cond_resched creates preemption windows** (mark corrected in ledger)
2. **Redesign NAT-001 harness** around Option 2 (timing-widening) not Option 1
3. **Implement false-sharing cache-line bouncing** as primary timing manipulation
4. **Re-evaluate statistical target**: 10k iterations may be insufficient; may need 100k-1M with timing widening

## Evidence Files
- This correction: `tier2/evidence/NAT-002/NAT-002_CORRECTION.md`
- Original analysis: `tier2/evidence/NAT-002/NAT-002_RESULTS.md`
- Raw disassembly: `tier2/evidence/NAT-002/NAT-002_raw_disassembly.log`