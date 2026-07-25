# Engineering Readiness Assessment: Android AVD PoC 

## Context & Baseline
The repository has achieved a highly mature state in its sterile QEMU environment. The core vulnerability primitive is fully deterministic, observable, and verified up to the exact moment of memory corruption (`ioctl_lock.wait_list.next` replaced with a non-NULL `epitem` pointer). The critical gap between the current state and **Goal 2 (Reproducible Android AVD PoC)** lies in bridging sterile dynamic observation with complex OS environmental constraints, and converting source-level theory (mutex slowpath consumption) into runtime fact.

---

## Remaining Milestones (Ranked by Engineering Value)

### 1. Milestone: Runtime Observation of Mutex Slowpath Dereference
* **Why it matters:** Currently, the repository relies on static source analysis to assert that `__mutex_lock_common` -> `__list_add` will consume the corrupted `wait_list`. In compiled kernel environments, inline expansions, fast-path lock optimizations, and compiled-out debug guards (`CONFIG_DEBUG_LIST`) can drastically alter expected execution paths.
* **Uncertainty it removes:** Proves definitively that the runtime execution environment naturally routes a contended `snd_timer_user` ioctl into the vulnerable slowpath, operating on the exact corrupted memory state without crashing preemptively.
* **Dependencies:** Verified `wait_list` corruption (Completed).
* **Expected evidence:** A deterministic GDB execution trace showing the CPU registers at the exact instruction `WRITE_ONCE(prev->next, new)` within `__list_add`, confirming `prev` resolves to the corrupted `epitem` pointer.
* **Repository artifacts involved:** 
  - `tier2/reproducers/test_timer_write.c` (requires multi-thread contention logic)
  - `tier2/scripts/gdb_timer_write_test.py` (requires new breakpoints at mutex slowpath)
  - `tier2/docs/VERIFICATION_LEDGER.md` (requires new entry)
* **Objective completion criteria:** A captured, reproducible runtime log demonstrating the target kernel instruction dereferencing the corrupted pointer.
* **Confidence:** 10/10. This is the absolute critical path. If the slowpath cannot be reached or behaves unexpectedly, the primitive is dead, rendering AVD integration irrelevant.

### 2. Milestone: Post-Corruption Primitive Stability Assessment
* **Why it matters:** Exploitation requires survival. If the mutex slowpath consumes the corrupted list but immediately triggers an unrecoverable kernel panic (e.g., via a subsequent read of a garbage pointer during the same lock operation), the bug is merely a Denial of Service.
* **Uncertainty it removes:** Determines whether the kernel can gracefully resume execution or reliably block the corrupted thread without bringing down the entire OS, allowing userland to continue executing the subsequent stages of a PoC.
* **Dependencies:** Runtime Observation of Mutex Slowpath Dereference (Milestone 1).
* **Expected evidence:** System logs (dmesg) and GDB traces showing the kernel returning to user space, or safely parking the thread in a `TASK_UNINTERRUPTIBLE` state without a generic protection fault.
* **Repository artifacts involved:** 
  - `tier2/reproducers/test_timer_write.c` 
  - `tier2/docs/RUNTIME_VALIDATION.md`
* **Objective completion criteria:** The reproducer successfully executes the UAF, triggers the stale write, forces the mutex slowpath, and the host QEMU instance remains interactive and responsive via the shell.
* **Confidence:** 9/10. Directly answers whether the primitive is usable for privilege escalation or if complex state-repair engineering is required immediately.

### 3. Milestone: Android OS Environment Integration (Transition to AVD)
* **Why it matters:** The current validation occurs in a sterile QEMU + BusyBox environment. A real Android Virtual Device introduces binder transactions, aggressive memory management, differing slab fragmentation, SELinux constraints, and background noise that can severely disrupt race determinism and heap grooming.
* **Uncertainty it removes:** Proves that the deterministic race orchestration and exact heap reclaim techniques developed in the sterile environment survive the transition to a noisy, heavily mitigated Android OS.
* **Dependencies:** Post-Corruption Primitive Stability Assessment (Milestone 2).
* **Expected evidence:** Automated scripts executing the reproducer via `adb shell` on a standard AVD, accompanied by `logcat` and `dmesg` outputs confirming the identical `wait_list` corruption observed in QEMU.
* **Repository artifacts involved:** 
  - `tier2/scripts/run_avd_test.sh` (New)
  - `tier2/docs/ENVIRONMENT.md` (Update for AVD specs)
* **Objective completion criteria:** The exact verification claims established in `VERIFICATION_LEDGER.md` (VER-001 through VER-008) can be reproduced on the AVD target with a single shell command.
* **Confidence:** 9/10. This is the definition of Goal 2, but must follow primitive verification to avoid conflating OS noise with fundamental primitive failures.

### 4. Milestone: Mitigation Impact and HW_TAGS Assessment
* **Why it matters:** Android GKI builds heavily utilize mitigations. As noted in `PROJECT_STATE.md`, the behavior of KASAN HW_TAGS (MTE) in the emulator must be definitively mapped. If MTE catches the UAF immediately, the exploit strategy must pivot to mitigation bypass before primitive construction.
* **Uncertainty it removes:** Clarifies whether hardware/software mitigations prevent the stale write from occurring in the AVD, defining the scope of required mitigation bypasses.
* **Dependencies:** Android OS Environment Integration (Milestone 3) or bare QEMU.
* **Expected evidence:** Comparative execution logs with mitigations enabled vs. disabled (`kasan=off`), documenting exact crash signatures or bypass success.
* **Repository artifacts involved:** QEMU/AVD boot configuration scripts, `tier2/docs/KNOWLEDGE_EVOLUTION.md`.
* **Objective completion criteria:** A documented matrix detailing how each enabled Android mitigation (KASLR, CFI, PAC, BTI, HW_TAGS) reacts to the reproducer.
* **Confidence:** 8/10. Necessary for a realistic PoC, but only relevant once the environment is transitioned.

---

## Technical Lead Directive

**If I were the technical lead, the exact next milestone I would approve tomorrow morning is:**

> **Milestone 1: Runtime Observation of Mutex Slowpath Dereference**

### Justification:

1. **Reduction of Unknowns:** In kernel engineering, source analysis is theory; runtime observation is fact. We are currently carrying a massive unverified assumption: that the compiled kernel actually routes our specific execution context through the vulnerable `__list_add` logic exactly as it appears in the C source. Inline expansions or unexpected lock fast-paths could instantly invalidate this theory. 
2. **Experimental Risk:** Transitioning to the AVD environment (Milestone 3) right now violates strict experimental methodology. If we migrate to a noisy AVD and the exploit fails, we will not know if the failure is due to Android OS noise (scheduling, slab fragmentation) OR because the mutex slowpath theory was fundamentally flawed. We must isolate variables.
3. **Repository Maturity:** The repository possesses highly mature, verified GDB orchestration (`gdb_timer_write_test.py`) tailored specifically for this sterile QEMU environment. It is vastly cheaper in engineering hours to extend this existing instrumentation to observe the mutex slowpath today, rather than trying to build brand new AVD observability infrastructure while simultaneously debugging a theoretical primitive.
4. **Evidence-Backed Progression:** Every claim in `VERIFICATION_LEDGER.md` currently rests on direct dynamic observation. The mutex slowpath claim currently rests *only* on static analysis. We must convert this final theoretical assumption into a verified, reproducible artifact (a GDB log) before advancing to complex exploitation environments.
