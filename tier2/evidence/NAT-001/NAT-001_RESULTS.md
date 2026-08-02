# NAT-001: Statistical Natural Race Test — Final Results

## Configuration
- Kernel: linux-6.12.67 (Android 14 GKI, commit 7e35917775b8)
- Config: PREEMPT_VOLUNTARY, SLUB_CPU_PARTIAL, HZ=1000, Dynamic Preempt
- QEMU: virt, cortex-a57, 2 CPUs, 2GB RAM
- cmdline: kasan=off nokaslr earlycon=pl011,0x09000000 printk.devkmsg=on rw

## Timing-Widening Techniques Applied
1. **False-sharing cache-line bouncing** (Thread C on CPU 0): 64-byte cache line hammered continuously
2. **Slab prefill** (5,000 eventpoll fds): fills kmalloc-192 cpu_partial lists (cpu_partial=120 objects)
3. **Timer/IPI storm** (Thread D on CPU 1): 100 timerfds at 1μs interval + sched_yield loop
4. **Multi-epitem topology**: outer epoll watches 2 inner epolls (epitem 1, epitem 2)

## Race Window (Per NAT-002 Analysis)
- Base instruction-count window: ~250-550 cycles between `__ep_remove(epitem_1)` and `ep_unregister_pollwait(epitem_2)`
- At 2GHz: ~125-275 nanoseconds
- No scheduling yield points (cond_resched is no-op in this topology)

## Results

| Boot | Hits | Iterations | Cumulative Hit Rate |
|------|------|------------|---------------------|
| 1    | 0    | 1000       | 0.000000%          |
| 2    | 0    | 1000       | 0.000000%          |
| 3    | 0    | 1000       | 0.000000%          |
| 4    | 0    | 1000       | 0.000000%          |
| 5    | 0    | 1000       | 0.000000%          |
| 6    | 0    | 1000       | 0.000000%          |
| 7    | 0    | 1000       | 0.000000%          |
| 8    | 0    | 1000       | 0.000000%          |
| 9    | 0    | 1000       | 0.000000%          |
| 10   | 0    | 1000       | 0.000000%          |

## Total
- **Total Hits**: 0
- **Total Iterations**: 10,000
- **Overall Hit Rate**: 0.000000%

## Wilson 95% Confidence Interval
- **Lower**: 0.000000
- **Upper**: 0.000384

## Conclusion
**RACE NOT NATURALLY WINNABLE** — 0 hits in 10,000 iterations with all four timing-widening techniques combined. The 95% CI upper bound of 0.0384% (p < 0.000384) means even if the race were naturally reachable, its probability is < 3.84e-4 per attempt.

This confirms the critical finding: the GDB-assisted race (VER-026) requires an artificial preemption point that does not exist naturally. The `cond_resched()` points in `ep_clear_and_put` (lines 888, 903) are no-ops in a 2-CPU pinned `PREEMPT_VOLUNTARY` setup because `TIF_NEED_RESCHED` is never set.

## Evidence Files
- Raw serial log: `tier2/evidence/NAT-001/qemu_serial.log`
- This summary: `tier2/evidence/NAT-001/NAT-001_RESULTS.md`