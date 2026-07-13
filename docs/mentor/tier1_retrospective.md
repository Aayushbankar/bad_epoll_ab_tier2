# Tier 1 Retrospective: Analysis of Environmental and Assumptions Failures

## 1. Overview
The purpose of Tier 1 was to reproduce the CVE-2026-46242 (Bad Epoll) exploit on a custom-compiled Linux 6.12.67 kernel. While the theoretical vulnerability remained the same, the practical application of the public PoC failed catastrophically at almost every stage. This retrospective analyzes those failures to demonstrate that modern kernel exploitation is hyper-dependent on environment stability.

## 2. Blocker Analysis

### A. KASLR Timing Leak Failure (`rdtscp` SIGILL)
* **What happened:** The exploit crashed the guest VM immediately with a `SIGILL` (Illegal Instruction) when attempting to execute the KASLR side-channel leak.
* **Why it happened:** The exploit relied on the `rdtscp` instruction for high-precision timing. Our local QEMU virtual machine was booted using the default `kvm64` CPU profile, which strips advanced host CPU features, including `rdtscp`, from the guest.
* **Cause:** Environmental difference & Incorrect Assumption (assuming standard x86_64 features are available in all virtualized environments).
* **Discovery:** QEMU/kernel logs showing a `SIGILL` exception precisely at the instruction pointer for `rdtsc_begin()`.
* **Missing Knowledge:** How QEMU CPU models abstract underlying hardware and mask CPUID features.
* **Actionable Lesson:** Never assume CPU instruction extensions are available across different hypervisor configurations.

### B. Race Condition Calibration & Timeout
* **What happened:** The exploit successfully calculated the target interrupt threshold but repeatedly timed out (after 300 seconds) trying to hit the race condition window.
* **Why it happened:** QEMU introduces massive overhead for system calls and context switches compared to bare metal or specialized hypervisors. The original PoC hardcoded its iteration loops (`RACE_DUP_CLOSE_ITERS=250`) and lookahead constants (`RACE_AHEAD_HI=4000`) based on KernelCTF timings. In our VM, the false-sharing cache window was drastically different, causing the exploit to consistently miss the race.
* **Cause:** Environmental difference.
* **Discovery:** The exploit ran cleanly without crashing but outputted `retries=14313` and timed out before landing the cross-cache allocation.
* **Evidence:** Logs confirming execution but failing the threshold check continuously.
* **Missing Knowledge:** The precise mechanics of how hypervisor scheduling impacts cache-line bouncing and false-sharing predictability.
* **Actionable Lesson:** Hardcoded timing loops are brittle. Exploits must dynamically calibrate cache-line bounce timings on the target host before attempting the race.

### C. Arbitrary Address Read (AAR) Page Fault
* **What happened:** The kernel paniced with a supervisor read page fault (`#PF`) inside `ep_show_fdinfo`.
* **Why it happened:** To achieve AAR, the exploit forges a fake `struct file` and overlaps it over a reclaimed pipe buffer page. To calculate the fake pointers, it relies on offsets for `init_task` and `task_struct` fields (e.g., `comm`, `files`). Because we compiled the kernel locally with a different `.config` than the KernelCTF target, the struct padding and layout shifted. The exploit read unmapped memory.
* **Cause:** Incorrect Assumption (assuming structural offsets are tied strictly to kernel version rather than build configuration).
* **Discovery:** GDB debugging of the `vmlinux` binary revealed that the `comm` field shifted from `0x788` (KernelCTF) to `0x730` (Our build).
* **Evidence:** GDB layout analysis showing the offset mismatches.
* **Missing Knowledge:** How specific compiler configurations (`CONFIG_SCHED_INFO`, `CONFIG_AUDITSYSCALL`, etc.) inject conditional fields into core structures like `task_struct`.
* **Actionable Lesson:** Struct layouts are highly volatile. Exploits must dynamically resolve offsets or require precise target database mappings.

### D. ROP / Stack Pivot Panic
* **What happened:** The kernel paniced when transitioning code execution to the payload.
* **Why it happened:** The original PoC relied on a specific JOP/ROP chain to pivot the stack. Our local GCC compilation optimized the `.text` segment differently. The expected thunk gadgets were missing, and the base offsets shifted entirely.
* **Cause:** Environmental difference.
* **Discovery:** Reversing the customized `vmlinux` using `objdump` and `ROPgadget` confirmed the original gadget address contained unrelated instructions.
* **Evidence:** Crash logs showing `#GP` or `#PF` at an unexpected Instruction Pointer during context switch.
* **Missing Knowledge:** Compiler generation of indirect branch thunks and how optimization flags shift `.text` segment alignments.
* **Actionable Lesson:** Hardcoded ROP chains are entirely deterministic to the exact compiler version and flags used to build the target kernel.

## 3. Summary
The failures in Tier 1 were not due to a misunderstanding of the epoll vulnerability, but a lack of appreciation for the environmental rigidity required by public PoCs. To proceed scientifically, we must first establish a baseline that eliminates these variables.
