# Learning Gaps and Knowledge Categories

This document categorizes the lessons learned during Tier 1, explaining why each area of knowledge is critical to this specific project, rather than providing generic definitions.

## Linux Kernel Internals
**Why it matters here:** We are attacking `epoll` and `timerfd`. To stretch the race condition window, we force an interrupt via a timer. Understanding the lifecycle of file descriptors, how the VFS layer handles `close()`, and the `epoll` notification subsystem is mandatory to understand why the UAF even exists and how to trigger it.

## Memory Management
**Why it matters here:** The kernel's SLAB/SLUB allocator dictates where objects live. Our exploit requires reclaiming a freed `struct eventpoll` cache line with an entirely different object (a pipe buffer page) via a cross-cache attack. If we do not understand slab fragmentation, partial/empty slabs, and cache merging, we cannot predictably reallocate the target memory.

## Kernel Structures
**Why it matters here:** The exploit uses an Arbitrary Address Read (AAR) by forging a fake `struct file`. If we don't know the precise byte layout of `struct file` and `task_struct`, our forged pointers will read unmapped memory, causing a `#PF` (Page Fault) supervisor crash as witnessed in Tier 1.

## Exploit Primitives
**Why it matters here:** A Use-After-Free (UAF) is just a bug. We must understand how to chain primitives: converting the UAF into a Cross-Cache overlap, converting that overlap into an Arbitrary Address Read (AAR) and Arbitrary Address Write (AAW) to finally corrupt the stack or `modprobe_path`.

## Race Conditions
**Why it matters here:** The entire bug is a race condition. It is non-deterministic. We must understand how false-sharing in CPU caches and precise system call delays can artificially stretch the race window, giving our malicious thread enough time to win.

## Cross-Cache Attacks
**Why it matters here:** The `eventpoll` object belongs to a specific cache. To gain control over its contents after it is freed, we must force the memory allocator to release the entire SLAB page back to the kernel, and then reclaim it using a generic allocator (like `kmalloc` via pipe buffers). This is the only way to forge the `struct file` safely.

## Assembly and Calling Conventions
**Why it matters here:** We are hijacking the instruction pointer (`RIP`). To safely pivot the stack pointer (`RSP`) into our controlled memory without crashing the kernel, we must understand x86_64 calling conventions and register states at the exact moment of execution flow hijacking.

## ROP Fundamentals
**Why it matters here:** The kernel employs SMEP (Supervisor Mode Execution Prevention) and SMAP (Supervisor Mode Access Prevention). We cannot simply execute shellcode in user space. We must build a Return-Oriented Programming (ROP) chain using existing kernel code (gadgets) to disable these mitigations or directly escalate privileges (e.g., `commit_creds(prepare_kernel_cred(0))`).

## Kernel Debugging with GDB
**Why it matters here:** When the exploit panics, the kernel dies. The only way to inspect memory layouts, identify shifted struct offsets (like our `comm` field shift), and verify ROP gadget locations in the `.text` segment is by attaching GDB to the `vmlinux` binary running in QEMU.

## QEMU and Virtualization
**Why it matters here:** Virtualization strips CPU features (like `rdtscp`) and radically alters system call latency. We must understand QEMU flags (`-cpu host`, `-smp`) to ensure our test environment doesn't artificially break timing-sensitive exploits designed for bare metal.

## Kernel Compilation and Configuration
**Why it matters here:** The `.config` file isn't just about features; it changes the size and layout of core structures. Compiler flags dictate the availability and location of ROP gadgets. Exploits are inextricably linked to the specific compilation environment of their target.
