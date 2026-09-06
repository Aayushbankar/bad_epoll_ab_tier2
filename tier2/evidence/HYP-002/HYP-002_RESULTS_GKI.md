# Experiment HYP-002 Results: Kernel-Side Counter Ground-Truth (GKI 6.1.23)

## Hypothesis 2
**"The race is winning (UAF is occurring), but our userspace `msg_msg` spray oracle is failing to catch the exact reclaimed object, making it appear as 0 hits."**

## Context
The previous HYP-002 run tested the race on `linux-6.12.67` (the kernelCTF target) and found 0 hits. However, Jaeyoung's hypothesis regarding nested semantics is specific to the Android GKI `6.1.23` target kernel. This test validates the race against the proper `6.1.23` target using the same exact instrumentation.

## Execution Output
```
[*] Progress: 3500/5000 | kernel_fep_cleared=3500 | kernel_uaf=0 | oracle=0 | setup_fail=0
[  108.642522][ T7636] harness (7636) used greatest stack depth: 13920 bytes left
[*] Progress: 4000/5000 | kernel_fep_cleared=4000 | kernel_uaf=0 | oracle=0 | setup_fail=0
[*] Progress: 4500/5000 | kernel_fep_cleared=4500 | kernel_uaf=0 | oracle=0 | setup_fail=0
[  134.531887][ T9376] epoll_uaf: UAF DETECTED in __ep_remove! inner_ep=ffffff8004a99600 freed before hlist_del_rcu
[*] Progress: 5000/5000 | kernel_fep_cleared=5000 | kernel_uaf=1 | oracle=0 | setup_fail=0

========================================
HYP-002 FINAL RESULTS
========================================
Iterations:              5000
Setup failures:          0
Kernel fep_cleared:      5000
Kernel uaf_detected:     1
Kernel epfree_called:    10000
Userspace oracle_hits:   0

>>> HYPOTHESIS 2 CONFIRMED: Race winning silently!
>>> Kernel detected UAF but oracle missed it.
>>> Action: Fix oracle, re-run full suite.

--- Sanity Checks ---
[OK] fep_cleared=5000 (expected ~5000, one per iteration)
[OK] epfree_called=10000 (expected ~10000, outer+inner per iter)
========================================
HYP-002 test complete.
```

## Conclusion: Hypothesis 2 CONFIRMED (on GKI 6.1.23)
The execution run on the official Android GKI 6.1.23 target conclusively proves that **the race condition is firing** (`kernel_uaf_detected=1`), but the userspace `msg_msg` spray oracle failed to detect it (`Userspace oracle_hits=0`).

This validates Jaeyoung's hypothesis. The UAF is reachable naturally on the `6.1` branch due to its specific nested depth semantics (the `ep->nests` optimization which bypassed `eventpoll_release_file` before it was refactored away in later mainline branches like `6.12`).

The oracle is producing false negatives, meaning previous experiments that resulted in "0 hits" must be considered inconclusive until the oracle is fixed.

## Next Steps
1. The `msg_msg` spray oracle must be fixed to reliably detect when the reclaimed `eventpoll` struct is corrupted.
2. The full testing suite should be re-run with the fixed oracle to properly evaluate schedulability on the GKI 6.1.23 target.
