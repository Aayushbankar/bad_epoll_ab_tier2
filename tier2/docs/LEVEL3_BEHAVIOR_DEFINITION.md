# LEVEL 3 Behavior Definition: CVE-2026-46242

This document defines precisely what constitutes a "LEVEL 3 — BEHAVIOR REPRODUCED" finding for CVE-2026-46242 on the target Android ARM64 kernel (commit `7e35917775b8b3e3346a87f294e334e258bf15e6`).

## Objective Distinction
This classification distinguishes between mere structural existence (LEVEL 0), successful compilation (LEVEL 1), environment booting (LEVEL 2), and the observation of actual memory corruption (LEVEL 4).

LEVEL 3 specifically measures whether the **temporal concurrency condition (the race window) can be reliably crossed** under the target architecture's timing topology, triggering the expected kernel-side logic flaw, but without requiring an explicit kernel panic or memory-safety exception (which falls under LEVEL 4).

## Definition of LEVEL 3

A test achieves LEVEL 3 if it satisfies ALL of the following criteria:

### A. The vulnerable source code exists
- `ep_remove()` and `eventpoll_release_file()` are present in the kernel image without backported synchronization fixes (already verified in LEVEL 0).

### B. The relevant syscall/API behavior works normally
- The test harness successfully allocates a target `file` structure and registers it with an `eventpoll` descriptor.
- Normal `epoll_ctl(EPOLL_CTL_DEL)` and `close()` operations succeed without immediate kernel faults when run sequentially.

### C. The concurrency condition is exercised
- The test harness executes concurrent threads attempting to trigger the exact Time-Of-Check to Time-Of-Use (TOCTOU) window described by the CVE.
- Specifically: Thread 1 invokes `close()` on the target file (entering `eventpoll_release_file`), and Thread 2 invokes `epoll_ctl(EPOLL_CTL_DEL)` on the epoll fd (entering `ep_remove`).

### D. A relevant abnormal kernel behavior is observed
- The test must yield an observable state confirming the race condition occurred. 
- In this CVE, the primary oracle for the race is measuring the CPU cycles required for `ep_remove` to execute. When the race window is hit, `ep_remove` will take significantly longer (or fail in a distinct way) because `eventpoll_release_file` is simultaneously manipulating the `f_ep` list or `ep->mtx` lock.
- If the test binary can reliably detect this precise cycle-count anomaly (the timing oracle) matching the known signature of the UAF vulnerability, then the vulnerability's logic path has been reached.

### E. Distinguishing from Memory-Safety Failure
- A LEVEL 3 reproduction **does not** require a kernel panic, KASAN splat, or memory corruption.
- If a KASAN splat for `eventpoll_epi` or a kernel panic explicitly mapping to `ep_remove` is observed, the reproduction immediately advances to **LEVEL 4 (MEMORY-SAFETY FAILURE OBSERVED)**.

## Validation Method
1. Compile a minimal C harness isolating *only* the `epoll_ctl` / `close` race and the cycle-counting timing oracle.
2. Exclude all object grooming (e.g., `msg_msg`), heap shaping, or arbitrary read/write ROP logic.
3. Run the harness under the QEMU environment.
4. If the harness reports the race was successfully timed/hit, LEVEL 3 is confirmed.
