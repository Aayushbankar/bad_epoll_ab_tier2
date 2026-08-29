# Deep Dive into CVE-2026-46242 ("Bad Epoll"): From x86_64 Exploitation to ARM64 Android GKI Dead Ends

**Author:** Aayush Bankar

Kernel exploitation is a domain where theory and practice frequently collide. A vulnerability that grants a clean privilege escalation on one architecture might be completely neutered by the mitigations of another.

Over the past weeks, I've conducted an intensive engineering deep dive into **CVE-2026-46242**, also known as "Bad Epoll." This vulnerability is a race condition in the Linux kernel’s event polling (`epoll`) subsystem that leads to a Use-After-Free (UAF). This post details my journey of fully reproducing the exploit on an x86_64 Linux environment (Tier 1) and the subsequent, highly educational failure when attempting to port it to an Android ARM64 Generic Kernel Image (Tier 2).

---

## The Vulnerability: Bad Epoll (CVE-2026-46242)

Introduced in Linux kernel v6.4-rc1 (commit `58c9b016e128`) and patched in v6.11 (`a6dc643c693`), Bad Epoll stems from a close-vs-close race condition. 

The core issue lies in the interaction between `__ep_remove()` and `eventpoll_release()`. When one eventpoll file descriptor monitors another, closing both concurrently can trigger a race:
1. `__ep_remove()` clears `file->f_ep` to `NULL` under a lock (`f_lock`).
2. A concurrent `__fput()` (triggered by file close) enters `eventpoll_release()`. It checks `f_ep` on a lockless fast path, sees `NULL`, assumes no epoll references exist, and frees the associated eventpoll and file objects.
3. Meanwhile, `__ep_remove()` continues executing and dereferences the now-freed pointers.

This results in two distinct Use-After-Free conditions:
*   A UAF on `struct epitem` (in the `eventpoll_epi` slab cache).
*   A UAF on `struct file`.

Because `epoll` is a fundamental, heavily relied-upon syscall for high-performance I/O, it cannot simply be disabled or blocked via seccomp or SELinux without breaking the OS. This makes it an incredibly attractive target.

---

## Tier 1: The x86_64 Validation Success

My first objective was to validate the exploit in a controlled, virtualized x86_64 environment (Linux 6.12.67 LTS on QEMU). Adapting an upstream exploit to a localized kernel build requires aligning offsets, dealing with toolchain friction, and understanding the core exploitation strategy.

### The Exploit Chain
The x86_64 exploit, originally submitted via Google kernelCTF, operates in several elegant stages:
1.  **Race Trigger:** Aggressively triggers the close-vs-close race, widening the window via false sharing and timer interrupts.
2.  **Cross-Cache & AAR:** The freed `file` struct is reclaimed by spraying `pipe_buffer` objects, granting full control over the memory region and establishing an Arbitrary Address Read (AAR) primitive via `/proc/self/fdinfo` by forging `f_inode`.
3.  **KASLR Leak:** The AAR is used to leak `task_struct` and discover the kernel base.
4.  **RIP Control:** The exploit hijacks the `file->f_op->poll` indirect call, using a stack pivot to transition into a Return-Oriented Programming (ROP) chain.
5.  **Privilege Escalation:** The ROP chain executes `commit_creds(&init_cred)` and `switch_task_namespaces()`, finally spawning a root shell (`execve(/bin/sh)`).

After mitigating offset desynchronization issues and rebuilding the environment, I successfully achieved a stable root shell. The Tier 1 environment demonstrated that the vulnerability is highly reliable (~99% success rate) when timing parameters are correctly tuned.

---

## Tier 2: The Android ARM64 Reality Check

The ultimate goal was to port this exploit to Android (Tier 2), specifically targeting the **Android 14 GKI (Generic Kernel Image) on ARM64**. If successful, this would represent a single-stage, sandbox-to-root pipeline—collapsing the traditional 3–4 stage Android exploit chain into one.

However, modern Android kernel mitigations are formidable. Over the course of 19 experiments and 21 documented dead ends, the exploitability of Bad Epoll on Android ARM64 systematically dismantled itself.

### The Mitigation Wall
Here is why the x86_64 exploit chain fails on Android ARM64 GKI:

1.  **Cross-Cache Defeated:** The Tier 1 exploit relies on reclaiming the freed `file` struct with a `pipe_buffer`. On the Android GKI, `filp_cachep` uses `SLAB_TYPESAFE_BY_RCU`. This heavily isolates the cache, and combined with a size class mismatch, makes the `pipe_buffer` cross-cache strategy impossible.
2.  **Control Flow Hijacking Blocked:** Even if we could forge a function pointer (e.g., `f_op->poll`), Android's hardware and software mitigations step in:
    *   **kCFI (Kernel Control Flow Integrity):** Validates indirect call targets dynamically.
    *   **PAC (Pointer Authentication Codes):** Cryptographically signs pointers, preventing arbitrary function pointer injection.
    *   **BTI (Branch Target Identification):** Restricts where indirect branches can land.
    Together, these mitigations completely block JOP, ROP, and indirect call hijacking.
3.  **The Schedulability Problem:** In the QEMU emulator, the race was easily triggered. However, a deeper timing analysis revealed that the real-world race window is roughly 250–550 CPU cycles (~125–275 ns). Without preemption points in `__ep_remove`, the race cannot be naturally scheduled. In 102,740 unaided attempts, the race triggered 0 times. While physical ARM64 silicon (with asymmetric cores and cacheline invalidation latency) might theoretically allow it to trigger, the resulting primitive is practically useless.

### The Final Primitive: A DoS-Only Verdict
Even assuming 100% race trigger success with hardware assistance, the only remaining memory corruption primitive we isolated was an **8-byte NULL write at offset 160** of a `kmalloc-192` slab. 

On the Android GKI 6.1, offset 160 either falls within user payload space (e.g., `msg_msg` payloads) or corrupts pointers in a way that strictly results in a kernel panic when dereferenced. With control-flow hijacking blocked and no viable data-only escalation path from this specific offset, the vulnerability is strictly a **Denial of Service (DoS)** on this configuration.

---

## Conclusion and Pivot

The Bad Epoll deep dive was a masterclass in the difference between a vulnerability and an exploit. What is a highly reliable root exploit on x86_64 Linux becomes a mere kernel panic on a hardened Android ARM64 device.

This negative result is scientifically defensible and highlights the efficacy of Android's modern mitigation stack (kCFI, PAC, BTI, and slab isolation).

**What's Next?**
While Bad Epoll met a dead end for privilege escalation, our parallel vulnerability research has identified a much more viable path: **vendor GPU driver primitives**, a deterministic vendor GPU driver UAF. Unlike Bad Epoll, the vendor GPU UAF is a deterministic page-level UAF (not a schedulable race), reachable from the untrusted app sandbox, and provides direct physical-page writes that inherently sidestep PAC, BTI, and kCFI.

The journey continues, pivoting from the core kernel to the rich, complex attack surface of vendor GPU drivers.

*Thanks for reading! If you are interested in kernel security, Android exploit development, or mitigation analysis, feel free to connect.*
