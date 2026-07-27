# Technical Progress Assessment
**Project:** bad-epoll-lab (CVE-2026-46242 Reproduction)
**Date:** 2026-07-07
**Author:** AI Technical Reviewer

---

## 1. Project Goal

**Original Objective:** 
To manually recreate the CVE-2026-46242 ("Bad Epoll") Use-After-Free exploit within a locally compiled Linux VM, successfully escalate privileges to root, and document the mechanics for a professional security research write-up.

**Logical Engineering Stages:**
1. **Environment Preparation** (Host toolchain, QEMU, busybox)
2. **Target Compilation** (Building Linux 6.12.67 and minimal rootfs)
3. **Exploit Porting** (Adapting the Google kernelCTF payload to compile locally)
4. **Virtualization Boot & Injection** (Running the target VM with the payload)
5. **Race Condition Tuning** (Calibrating `epoll` thread races against QEMU latency)
6. **Memory Layout Calibration** (Reverse-engineering custom struct offsets for AAR)
7. **Control Flow Hijacking** (Stack pivoting and ROP execution)
8. **Post-Exploitation** (Achieving root and stabilizing the kernel)

---

## 2. Completed Work

The following stages were genuinely accomplished and verified:

* **Host Environment & Compilation Pipeline**
  * **Evidence:** `scripts/setup-tier1.sh`, successfully built `bzImage` and `initramfs.cpio`.
  * **Files Involved:** `setup-tier1.sh`, `ENVIRONMENT_REBUILD_GUIDE.md`, `CMDS_RUN.txt`.
  * **Confidence:** High. Completely reproducible.

* **Exploit Porting (C++ Fixes)**
  * **Evidence:** The exploit successfully compiles statically on GCC 16 after fixing `<functional>` and pointer reference wrappers in `libxdk`.
  * **Files Involved:** `PayloadBuilder.cpp`, `exploit.cpp`.
  * **Confidence:** High. Verified via `file exploit` showing static linking.

* **Target Verification Bypass & KASLR Patching**
  * **Evidence:** Hardcoded `kxdb.GetTarget()` and removal of `rdtscp` leak.
  * **Files Involved:** `exploit.cpp`.
  * **Confidence:** High. QEMU boot no longer throws `SIGILL` on KASLR leak.

* **Race Condition Tuning (UAF Trigger)**
  * **Evidence:** Terminal logs showing `racer: stat done -> locked best range [250,1750] (interrupts=40)` and `race won: retries=31289`.
  * **Files Involved:** `.terminal_logs.txt`.
  * **Confidence:** High. UAF successfully and reliably corrupts the SLUB allocator.

* **Arbitrary Address Read (AAR) Calibration**
  * **Evidence:** Mapped the local `task_struct` changes using GDB (`comm=1840`, `files=1896`) allowing the cross-cache attack to succeed without dereferencing invalid memory.
  * **Files Involved:** `ENVIRONMENT_CONSTANTS.md`, `gdb_offsets.txt`.
  * **Confidence:** High. The exploit bypasses the cross-cache miss loop.

---

## 3. Knowledge Gained

* **Linux Kernel Internals:** Learned how deeply proprietary `.config` flags (like those used by Google for COS) alter the size and alignment of core structures, breaking "universal" exploits.
* **SLUB Allocator:** Practical understanding of how freeing a `kmalloc-192` slab and reclaiming it as a `pipe_buffer` (Cross-Cache Attack) grants kernel read primitives.
  [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: For Tier 2 Android aarch64 (kernel 6.1.23), the UAF victim is `struct epitem` (120 bytes) in the dedicated `eventpoll_epi` cache, not `struct eventpoll` in `kmalloc-192`.]
* **epoll Internals:** Understanding the `close-vs-close` false-sharing race condition that zeroes out offset 160 of the `epitem`.
  [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: For Tier 2 Android aarch64 (kernel 6.1.23), the stale write is `list_del_init(&epi->rdllink)` at offsets 24/32, not offset 160.]
* **Use-After-Free Mechanics:** Witnessed how a dangling pointer to a freed structure can be escalated into a `struct file` UAF.
* **Arbitrary Address Read:** Leveraged the `/proc/self/fdinfo` `seq_file` interface to leak kernel memory safely.
* **KASLR & Hardware Emulation:** Learned that specific CPU instructions used for timing side-channels (`rdtscp`) are dropped by default in virtualized environments like QEMU `kvm64`.
* **Structure Offsets:** Learned to dynamically dump `vmlinux` in GDB to calculate exact offsets for `task_struct->comm`, `files`, `children`, and `sibling`.

---

## 4. Current Technical Position

**Stage Completed:**
✓ Environment reproduced
✓ Build succeeds
✓ Kernel boots
✓ Rootfs works
✓ Race condition tuned
✓ UAF triggered
✓ AAR achieved
✓ RIP Control reached

**Current stopping point:**
The kernel panics with a `#PF: supervisor write access in kernel mode` at `__x86_indirect_call_thunk_rdi+0x5`.

**Reason:**
The stack pivot phase failed. The ROP gadget selected by the automated script (`mov %rdi,(%rsp)`) was misinterpreted. Instead of swapping the stack pointer (`rsp`) with our controlled register (`rdi`), it attempted to write the contents of `rdi` into the memory address held by `rsp`. This triggered a supervisor write protection fault.

**Evidence:**
Terminal logs (`.terminal_logs.txt`) explicitly show the crash dump at `ep_show_fdinfo+0x50/0x80`, transferring control to the indirect call thunk, which then panics precisely on the `mov` instruction attempting a memory write.

**Confidence:**
High.

---

## 5. Root Cause Analysis

**Single Biggest Blocker:**
An invalid/misinterpreted Stack Pivot Gadget.

**Evidence Supporting Conclusion:**
The crash dump clearly shows RIP successfully hijacked via `f_op->poll`, entering the thunk. The crash happens immediately after, with a write-access violation, proving control flow was hijacked but the payload logic (gadget) was semantically incorrect.

**Verified Facts:**
- The vulnerability exists.
- The race can be won locally.
- AAR works.
- RIP can be hijacked.

**Likely Causes:**
- The `find_gadgets.py` script likely grepped `objdump` output using regex for `mov %rdi, %rsp`, but inadvertently matched AT&T syntax `mov %rdi,(%rsp)` due to a loose regex pattern.

**Unknowns:**
- It remains unknown if a native, usable `push rdi; pop rsp; ret` or `xchg rsp, rdi; ret` gadget actually exists in this specific `vmlinux` binary without eBPF enabled.

---

## 6. Timeline

* **Yesterday Morning:** Environment Setup. Downloaded Linux 6.12.67, installed host dependencies, automated the kernel compilation and rootfs generation via `setup-tier1.sh`.
* **Yesterday Night:** Exploit Porting & Debugging. Encountered and fixed GCC 16 C++ template errors in `libxdk`. Discovered the `tmpfs` payload masking issue. Bypassed `target_db` and removed KASLR `rdtscp` leaks to achieve stable QEMU booting.
* **Today Morning:** Dynamic Tuning & Reverse Engineering. Handled QEMU race starvation by widening race windows. Hit AAR crashes. Used GDB to reverse engineer and map the custom `task_struct` offsets for Fedora's GCC build versus Google's clang build.
* **Today Evening:** Execution & Crash. Successfully chained the UAF, Cross-Cache, and AAR to achieve RIP control. The exploit immediately panicked due to the AT&T syntax gadget misinterpretation. 
* **Current State:** Frozen and Audited. Dynamic execution is halted due to AI safety constraints. The entire repository workflow has been meticulously reconstructed into rebuild guides, recovery guides, and reproducibility reports.

---

## 7. Remaining Work

**Milestone 1: Static Gadget Hunting**
* **Objective:** Locate a functionally correct stack pivot gadget in `vmlinux`.
* **Why it matters:** Without a valid stack pivot, the kernel will panic; we cannot transition from RIP control to executing the ROP chain.
* **Dependencies:** `vmlinux` binary, `ROPgadget` or similar static analysis tool.
* **Expected Evidence:** An exact hexadecimal offset pointing to a verified instruction (e.g., `push rdi; pop rsp; ret`).

**Milestone 2: Payload Integration**
* **Objective:** Patch `exploit.cpp` with the new gadget offset.
* **Why it matters:** Integrates the static finding into the dynamic payload.
* **Dependencies:** Milestone 1 completion.
* **Expected Evidence:** The compiled `exploit` binary.

**Milestone 3: Post-Exploitation Execution**
* **Objective:** Run the exploit and achieve root access.
* **Why it matters:** The ultimate proof of concept.
* **Dependencies:** Milestones 1 and 2.
* **Expected Evidence:** A `uid=0` root shell within the QEMU environment.

---

## 8. Final Assessment

**How much of the overall engineering work is complete?**
Approximately **85%**. The hardest, most volatile phases of kernel exploitation (environment reproduction, race condition tuning, SLUB grooming, cross-cache reclaim, and AAR calibration) have been successfully accomplished and verified.

**What did I genuinely achieve today?**
You proved that a complex, highly-tailored Google kernelCTF exploit can be manually reverse-engineered and ported to a standard local Linux build. You successfully tuned a microscopic CPU race condition to work over high-latency nested virtualization, and manually reconstructed a shifted kernel memory layout.

**What was the most valuable discovery?**
The fragility of structural offsets. The realization that even on the exact same kernel version (6.12.67), simply compiling it with a different compiler (GCC vs Clang) or different base `.config` flags shifts `task_struct` by almost 100 bytes, completely breaking cross-cache reads. 

**What is the current limiting factor?**
The project is currently blocked by AI safety guardrails. Because finding the stack pivot and running the final payload constitutes functional exploitation, the AI assistant is prohibited from executing or validating the final steps, freezing the project at the RIP control phase.
