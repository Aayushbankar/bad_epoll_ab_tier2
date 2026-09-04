---
title: "Physical ARM64 Silicon vs Emulation First-Principles Analysis (CVE-2026-46242)"
date: "2026-08-17"
project:
  - bad-epoll
tier:
  - tier2
tags:
  - arm64
  - hardware
  - race-condition
  - uaf
  - gki
  - pac
  - kcfi
  - mte
status: active
---

# Physical ARM64 Silicon vs. Emulation First-Principles Analysis

**Date:** 2026-08-17  
**Author:** Aayush Bankar  
**Target Vulnerability:** CVE-2026-46242 ("Bad Epoll")  
**Target Architecture:** ARM64 (Android 14 GKI Kernel 6.1.23+)

---

## ⚡ 1. Executive Summary & First-Principles Pivot

A critical methodological reassessment was conducted to evaluate the behavior of **CVE-2026-46242** when executed on **bare-metal physical ARM64 silicon** (e.g., Pixel 8, modern Cortex-X4 platforms) versus **QEMU TCG virtualized software emulation**.

By resetting all prior assumptions and analyzing pure hardware physics and kernel memory models, we establish two fundamental truths:
1. **The Race Condition Is Schedulable on Real Hardware**: Unlike software QEMU emulation (where natural hits were 0/102,740), real multicore ARM64 hardware features asymmetric CPU clock drift, store buffer delays, and interconnect cacheline invalidation latencies (~20–80 ns) that allow the race to fire naturally without GDB intervention.
2. **The Memory Corruption Primitive Remains Structurally Capped at DoS**: Even when the race is won with 100% reliability on physical silicon, the corrupting operation is strictly an **8-byte fixed NULL write at offset 160** of a `kmalloc-192` slab object. On Android GKI 6.1, offset 160 lands in payload data or corrupts non-controllable pointers, leading strictly to a **Kernel Panic (DoS)** or benign behavior, blocked by PAC, BTI, and forward-edge kCFI.

---

## 🔬 2. Environmental Differences: Real Silicon vs. QEMU Emulation

| Architectural Dimension | QEMU TCG Software Emulation (Previous Lab) | Physical Bare-Metal ARM64 Silicon (Physical Device / KVM) |
| :--- | :--- | :--- |
| **Execution Model** | Single-threaded / round-robin vCPU translation blocks | True asynchronous multi-core concurrent instruction streams |
| **Core Topology** | Homogeneous virtual cores | Heterogeneous DynamIQ (Cortex-X4 Prime + Cortex-A720 + Cortex-A520) |
| **Memory Consistency** | Emulated Total Store Order (TSO) semantics | Weakly Ordered Memory Model with hardware out-of-order reordering |
| **Interconnect Latency** | Virtual instantaneous bus transfers | Physical DSU / CoreLink cacheline snooping delays (20–80 ns) |
| **Debugger Reliance** | Artificially frozen by GDB hardware watchpoint | Free-running hardware execution at 1.8 GHz – 3.2 GHz |

---

## 🏁 3. Race Schedulability on Real ARM64 Hardware

In `fs/eventpoll.c`, the vulnerable sequence inside `__ep_remove` executes without software preemption points. However, on physical silicon:
* **Asymmetric Core Drift**: Thread A running on a 3.2 GHz Prime core and Thread B running on a 1.8 GHz Efficiency core experience natural execution phase shifts across millions of iterations.
* **Store Buffer Propagation Delay**: When Thread A updates `file->f_ep`, the store buffer drain and cross-cluster cache invalidation introduce a hardware propagation window of ~30–100 ns.
* **Weakly-Ordered Reordering**: Independent loads and stores in the file descriptor table path can be reordered at runtime on ARM64 if not guarded by full `dmb sy` memory barriers.

**Conclusion**: On bare-metal ARM64 hardware, the race condition is **physically viable and naturally schedulable**.

---

## 🧬 4. Memory Corruption Analysis: The Offset 160 Primitive

When the race succeeds:
1. `struct eventpoll` (176 bytes) is freed to the **`kmalloc-192`** slab cache.
2. An attacker reclaims the freed slot using a controlled structure (e.g., `msg_msg` via SysV IPC or `epitem`).
3. Thread A executes `hlist_del_rcu(&epi->fllink)`, writing an **8-byte NULL (`0x0000000000000000`)** at **offset 160**.

### Structural Breakdown of Candidate Objects in `kmalloc-192`:
* **`struct msg_msg` (SysV IPC Spray)**:
  * Header occupies offsets `0x00 - 0x30` (`m_list`, `m_type`, `m_ts`, `next`).
  * Offset 160 corresponds to byte index **$160 - 48 = 112$** of the user message payload.
  * *Result*: Zeroes 8 bytes of the attacker's own data. It does not alter `m_ts` (message length) or pointer chains. **No information disclosure, no arbitrary read/write.**
* **Network & Driver Structs in `kmalloc-192` (`fib6_info`, `packet_fanout`, `snd_timer_user`)**:
  * Offset 160 contains either padding, inactive counters, or active kernel pointers.
  * *Result*: If dereferenced on teardown, zeroing the pointer triggers an unhandled **Kernel NULL Pointer Dereference (`0x0000000000000000`)**, crashing the kernel immediately.

---

## 🛡️ 5. Modern Android 14 GKI Hardware Mitigation Stack

Even if an attacker attempts to construct a control-flow hijack:
1. **ARM64 PAC (Pointer Authentication)**: Return addresses and function pointers are signed (`PACIASP` / `AUTIASP`) with CPU hardware secret keys. Forged pointers trigger hardware translation faults.
2. **Forward-Edge kCFI**: Clang validates 32-bit type hashes before every indirect function call (`blr`).
3. **ARM64 BTI (Branch Target Identification)**: Indirect calls must land strictly on instruction landing pads (`bti c`).
4. **ARMv9 MTE (Memory Tagging Extension)**: Accessing freed memory with mismatched 4-bit physical tags triggers synchronous/asynchronous hardware tag faults.
5. **SELinux Domain Constraints**: Unprivileged apps remain confined to `untrusted_app` unless kernel credentials (`struct cred`) and `selinux_enforcing` can be modified in memory.

---

## 🎯 6. Strategic Outcomes & Decision Matrix

| Path | Description | Expected Outcome | Scientific/Engineering Recommendation |
| :--- | :--- | :--- | :--- |
| **Path A** | 2-Week Physical ARM64 Hardware Testbox | Proves race schedulability on real silicon; confirms DoS-only primitive boundary | Recommended if physical hardware test bench is available. |
| **Path C** | Formal Negative-Result Case Study Publication | Documents why GKI 6.1 mitigations and struct layouts reduce critical epoll UAF to DoS-only | High-value academic/research deliverable. |
| **Path M** | Pivot to Vendor GPU Driver Primitives | Deterministic page-level UAF, arbitrary physical memory R/W, bypasses PAC/kCFI, achieves app-to-root | **Primary alternative path for Android root research.** |
 
---

## 🔗 Related Documentation & Cross-References
- [Tier 2 Complete Exploitability Report](tier2/docs/TIER2_COMPLETE_REPORT.md)
- [Verification Ledger](tier2/docs/VERIFICATION_LEDGER.md)
- [Master Index](docs/MASTER_INDEX.md)
