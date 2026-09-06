# Experiment HYP-002 Results: Kernel-Side Counter Ground-Truth (GKI 6.1.23)

## Status: INCONCLUSIVE — Race Detected Once, Not Reproducible Under QEMU TCG

## Hypothesis 2
**"The race is winning (UAF is occurring), but our userspace `msg_msg` spray oracle is failing to catch the exact reclaimed object, making it appear as 0 hits."**

---

## 1. Initial Finding (Commit `cf24bd6`)
In commit `cf24bd6`, a single test run of 5,000 iterations on Android GKI `6.1.23` recorded:
```
[  134.531887][ T9376] epoll_uaf: UAF DETECTED in __ep_remove! inner_ep=ffffff8004a99600 freed before hlist_del_rcu
[*] Progress: 5000/5000 | kernel_fep_cleared=5000 | kernel_uaf=1 | oracle=0 | setup_fail=0
Kernel uaf_detected:     1
Userspace oracle_hits:   0
```
This was initially marked as `CONFIRMED` on GKI 6.1.23.

---

## 2. Correction of the "False-Positive" Explanation
An earlier session attempted to dismiss the single hit as a "false positive from uninitialized `debug_freed`." That claim is **technically invalid**:
- In `tier2/android/source/common/fs/eventpoll.c:1021`, `ep_alloc()` allocates `struct eventpoll` via:
  ```c
  ep = kzalloc(sizeof(*ep), GFP_KERNEL);
  ```
- `kzalloc()` guarantees zeroed memory on every allocation (`GFP_KERNEL | __GFP_ZERO`).
- In the disassembled ARM64 kernel binary (`do_epoll_create`), the allocation translates to `kmalloc_trace` called with flag `w1 = 0xdc0` (`__GFP_ZERO`), allocating 184 bytes fully cleared.
- A stale `0xDEADFEED` value from a previously-freed object in slab cannot survive `kzalloc()`.
- The added `ep->debug_freed = 0` line is **redundant** (as `kzalloc` already zeroes the struct) but harmless.
- The kernel-side detection mechanism (`debugfs` counters and `pr_warn` check in `__ep_remove`) is completely sound.

---

## 3. Kernel Binary Comparison (`cf24bd6` vs `15a461370`)
Disassembly of `eventpoll.o` across the two builds:
- **`cf24bd6` (Build #1, Sun Sep 6 12:22:40 IST 2026)**:
  `ep_alloc` inlined into `do_epoll_create`. Allocation handled via `kmalloc_trace` (`0xdc0` = `__GFP_ZERO`). No explicit `str wzr` to offset 176.
- **`15a461370` (Build #2, Sun Sep 6 12:58:57 IST 2026)**:
  Identical except for one added redundant instruction at `0xf7c`:
  ```assembly
  f7c: b900b27f   str wzr, [x19, #176]
  ```
- **Critical path comparison**:
  The core racing functions (`__ep_remove` at `0xa30`, `ep_free` at `0x310`) and waitqueue operations are byte-for-byte identical in code generation between both builds. The non-reproducibility across runs is not due to any code-generation regression, but rather the intrinsic timing nondeterminism of QEMU TCG software emulation.

---

## 4. Reproduction Experiment (3 × 5,000 = 15,000 Iterations)
To test reproducibility, HYP-002 was re-run three consecutive times (15,000 total iterations) on Android GKI 6.1.23 under QEMU TCG:

- **Run 1** (`tier2/evidence/HYP-002/HYP-002_repro_run1.log`):
  ```
  Iterations:              5000
  Kernel fep_cleared:      5000
  Kernel uaf_detected:     0
  Kernel epfree_called:    10000
  Userspace oracle_hits:   0
  ```
- **Run 2** (`tier2/evidence/HYP-002/HYP-002_repro_run2.log`):
  ```
  Iterations:              5000
  Kernel fep_cleared:      5000
  Kernel uaf_detected:     0
  Kernel epfree_called:    10000
  Userspace oracle_hits:   0
  ```
- **Run 3** (`tier2/evidence/HYP-002/HYP-002_repro_run3.log`):
  ```
  Iterations:              5000
  Kernel fep_cleared:      5000
  Kernel uaf_detected:     0
  Kernel epfree_called:    10000
  Userspace oracle_hits:   0
  ```

### Cumulative Results on GKI 6.1.23
| Series | Iterations | Kernel UAF Detected | Userspace Oracle Hits |
|---|---|---|---|
| Original Run (`cf24bd6`) | 5,000 | **1** | 0 |
| Repro Run 1 (`15a461370`) | 5,000 | **0** | 0 |
| Repro Run 2 (`15a461370`) | 5,000 | **0** | 0 |
| Repro Run 3 (`15a461370`) | 5,000 | **0** | 0 |
| **Total Cumulative** | **20,000** | **1** (0.005%) | **0** |

---

## 5. Accurate Conclusion
1. **The race CAN fire on GKI 6.1.23 under QEMU TCG**: A genuine detection was captured in the original run (`inner_ep=ffffff8004a99600 freed before hlist_del_rcu`).
2. **The race is extremely rare**: It could not be reproduced across 15,000 additional iterations (1 hit in 20,000 total iterations, or ~0.005% hit rate).
3. **Hypothesis 2 Status**: **INCONCLUSIVE** — the race was detected once, proving the timing window can theoretically align under emulation, but its occurrence is too rare under QEMU TCG to be practically exploitable or reliably measured.
4. **Oracle Evaluation**: Because the single kernel hit produced 0 oracle hits, the oracle may indeed suffer from narrow reclaim timing or missed objects when the rare race does hit, but the primary bottleneck remains the race schedulability itself under TCG.

---

## Evidence References
- Original 1-hit log: Commit `cf24bd6` (`tier2/evidence/HYP-002/HYP-002_raw_serial.log`)
- Repro Run 1: `tier2/evidence/HYP-002/HYP-002_repro_run1.log`
- Repro Run 2: `tier2/evidence/HYP-002/HYP-002_repro_run2.log`
- Repro Run 3: `tier2/evidence/HYP-002/HYP-002_repro_run3.log`
- Repro Script: `tier2/scripts/run_hyp002_repro.sh`
- Kernel Patch: `tier2/scripts/hyp002_gki_kernel.patch`
