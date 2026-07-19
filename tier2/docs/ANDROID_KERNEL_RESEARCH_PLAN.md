# Android Kernel Research Plan (Knowledge Map)

This document breaks the Android kernel into discrete research modules required to understand the CVE-2026-46242 adaptation constraints. This is a *study plan*, not an exploit plan.

## Module 1: The GKI Memory Allocator (SLUB)
- **What it does**: Manages all generic kernel memory allocations (`kmalloc`, dedicated caches) using the SLUB algorithm.
- **Why it matters**: Predictable heap layout is the absolute foundation of Use-After-Free exploitation.
- **Relation to CVE**: The `epitem` structure is freed and reallocated. If we cannot predict *where* it is reallocated, or what we can overlap it with, the exploit fails immediately.
- **Difficulty**: High (Hardened SLUB with Randomization).
- **Priority**: Critical (Must be solved first).
- **Dependencies**: None.

## Module 2: Hardware Control-Flow Mitigations (PAC/BTI)
- **What it does**: ARMv8.3+ Pointer Authentication Codes (PAC) signs function pointers. ARMv8.5+ Branch Target Identification (BTI) restricts indirect jump targets.
- **Why it matters**: Traditional Linux exploits rely on overwriting function pointers (like `f_op->poll`) to redirect execution to ROP/JOP chains.
- **Relation to CVE**: The Tier 1 exploit directly overwrote `f_op->poll`. This is impossible on GKI.
- **Difficulty**: Very High.
- **Priority**: Critical.
- **Dependencies**: Module 1 (Need memory read/write first to interact with pointers).

## Module 3: Data-Only Escalation Primitives
- **What it does**: Modifies kernel data structures (`task_struct`, `cred`, `namespace`) without hijacking control flow (instruction pointer).
- **Why it matters**: Bypasses PAC, BTI, and CFI entirely by achieving the goal (privilege escalation) through logic corruption rather than code execution.
- **Relation to CVE**: We must pivot from control-flow hijacking to data-only escalation.
- **Difficulty**: Medium.
- **Priority**: High.
- **Dependencies**: Module 1.

## Module 4: The Epoll & VFS Lifecycle
- **What it does**: Manages file descriptors, wait queues, and asynchronous event notifications.
- **Why it matters**: We must fully understand the race condition and locking mechanisms to trigger the Use-After-Free reliably.
- **Relation to CVE**: This is the core vulnerability.
- **Difficulty**: Medium.
- **Priority**: Medium (We already proved it in Tier 1, but GKI preemption timing may differ).
- **Dependencies**: None.

## Module 5: Android IPC (Binder)
- **What it does**: Handles almost all inter-process communication on Android.
- **Why it matters**: Binder is the most powerful tool for shaping the kernel heap from userspace.
- **Relation to CVE**: Can potentially be used to spray objects or retrieve leaked pointers.
- **Difficulty**: High.
- **Priority**: Low (Only needed if standard `msg_msg` or `pipe_buffer` primitives fail).
- **Dependencies**: Module 1.
