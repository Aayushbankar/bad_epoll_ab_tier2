# DRAFT — Internal Progress Report — NOT for external distribution

**Status**: Work in progress, pending hardware access / further natural-reachability testing / final decision on Path A vs B vs C. To be revisited for external outreach only once a working result or a settled final conclusion exists.

*(Note: This is a living document meant to be updated in place as new experiments complete—such as AND-002/003/004 or hardware-access work—rather than a one-time deliverable. Future sessions should revise this file iteratively.)*

## 1. Abstract
This document provides an interim progress snapshot of the Tier 2 exploitability assessment for CVE-2026-46242 (epoll UAF) on Android ARM64 GKI (`linux-6.12.67`). We have successfully isolated the core UAF mechanism, reclaimed the freed memory reliably with `msg_msg`, and systematically mapped the constraints of the race window under QEMU emulation. However, achieving natural schedulability and constructing a viable exploitation chain remain open challenges. It is explicitly noted that this report represents a work-in-progress state rather than a final result.

## 2. Background
This research builds upon the original CVE-2026-46242 ("Bad Epoll") disclosure by J-jaeyoung (available at [J-jaeyoung/security-research](https://github.com/J-jaeyoung/security-research)). While the initial disclosure demonstrated a successful x86_64 local privilege escalation, the goal of this phase of work (Tier 2) is to port the exploit and rigorously validate the exploitability of the underlying UAF mechanism on a modern Android ARM64 Generic Kernel Image (GKI).

## 3. Methodology
Our build environment utilizes the standard AOSP Kleaf/Bazel toolchain targeting `linux-6.12.67` (Android 14 GKI, commit `7e35917775b8`), executed under QEMU with GDB introspection. A strict evidence discipline governs all claims, codified in [EXPERIMENT_PROTOCOL.md](tier2/docs/EXPERIMENT_PROTOCOL.md). By these non-negotiable rules, every technical claim must be explicitly backed by committed, raw runtime or static logs, distinguishing structurally verified phenomena from unproven theories to ensure rigorous tracking for future review.

## 4. Confirmed Findings (RUNTIME Evidence)
Through precise hardware watchpoint tracing (VER-026, [EXP-015_unified_trace.log](tier2/evidence/EXP-015_unified_trace.log)), we verified the exact UAF mechanism: a two-threaded race where Thread A clears `f_ep`, Thread B bypasses `eventpoll_release_file` via a lockless fast-path and frees `inner_epoll` (a 176-byte `struct eventpoll` in `kmalloc-192`), and Thread A resumes to execute `hlist_del_rcu`, writing `NULL` to offset 160 of the freed object. Using `msg_msg` allocations (144 bytes of user payload), we achieved reliable reclaim of this freed `kmalloc-192` slot, confirming attacker control over bytes from offset 48 onward (VER-027, [EXP-018_raw_gdb.log](tier2/evidence/EXP-018_raw_gdb.log)). Additionally, SysV IPC (`msgsnd`/`msgrcv`) is fully accessible under the target Android kernel context, validated via a trapped runtime call at `load_msg` (VER-038, [AND-001_raw_ipc.log](tier2/evidence/AND-001_raw_ipc.log)).

## 5. Systematic Exploitation Search and Negative Results
We systematically exhausted four potential exploitation chains, converting them into proven dead ends:
*   **Chain 0**: The hypothesis that `percpu_counter_dec` dereferences the freed `ep->user` was disproved; the parameter is always the valid *outer* epoll (VER-028, [EXP-019_raw_gdb.log](tier2/evidence/EXP-019_raw_gdb.log)).
*   **Chain 1**: The dual-watch KASLR leak was disproved because single-epitem (required for UAF) and multi-epitem (required for kernel pointer write) conditions are mutually exclusive (VER-033, [EXP-024_raw_gdb.log](tier2/evidence/EXP-024_raw_gdb.log)).
*   **Chain 2**: The arbitrary decrement via a fake `user_struct` was disproved as the operation strictly affects the outer epoll's user (VER-031/032, [EXP-023b_raw_gdb.log](tier2/evidence/EXP-023b_raw_gdb.log)).
*   **Chain 3**: Full LPE requires primitives from Chain 1 or 2, and a systematic audit found no exploitable 129-192 byte structs for a fixed NULL write at offset 160, restricting the remaining primitive to local DoS ([EXP-016 audit](tier2/docs/EXP-016_RESULTS.md)).

## 6. The Natural-Reachability Question
A critical open question remains whether the race is naturally schedulable without debugger assistance. Static and runtime analyses confirmed that `cond_resched()` near the race window is a no-op due to `PREEMPT_VOLUNTARY` and dynamic static keys (VER-035, [NAT-002_RESULTS.md](tier2/evidence/NAT-002/NAT-002_RESULTS.md)). The true race window is extremely narrow, purely instruction-timing limited to approximately 250-550 cycles. Under a closed-loop sweep within QEMU, we achieved 0 hits in 102,740 attempts (VER-039, [NAT-005_raw_serial.log](tier2/evidence/NAT-005_raw_serial.log)). This is a legitimate negative result under QEMU's TCG software emulation, but we acknowledge the honest environmental limitation: TCG does not model real cache-coherency or memory-bus timing characteristics of physical ARM64 chips.

## 7. Current Status and Open Paths
As outlined in the mentor executive summary, we evaluate three paths forward: Path A (hardware timing-widening), Path B (alternative race variant), and Path C (conclude DoS-only). We are currently pursuing **Path A** under a strict 2-week timebox on physical ARM64 hardware to determine if false-sharing cache bouncing, slab contention, or IPI/timer storms can widen the narrow race window. To consider this ready for external review or outreach, this timebox must either produce reliable, naturally scheduled hits (validating exploitability), or definitively reach zero hits across ~1 million iterations, prompting a final pivot to Path C (concluding DoS-only) and a finalized technical writeup.

## 8. Reproducibility
All research artifacts, test harnesses, and raw execution logs are tracked in this repository for internal validation. All claims are indexed in [VERIFICATION_LEDGER.md](tier2/docs/VERIFICATION_LEDGER.md) with supporting physical evidence available in the `tier2/evidence/` directory.
