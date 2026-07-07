# Status Report
**To:** Rathod Ruturaj Prafulsin (CypherMatrix Mentor)
**From:** Ayush
**Date:** 2026-07-07
**Project:** bad-epoll-lab (CVE-2026-46242 Recreation)

## 1. Work Completed
* **Tier 1 Infrastructure:** Successfully compiled Linux Kernel 6.12.67 from source on a Fedora host, created a minimal `busybox` root filesystem, and established a stable QEMU execution pipeline.
* **Exploit Porting:** Ported the original Google kernelCTF exploit to our custom environment. Resolved modern GCC 16 C++ standard compilation errors within `libxdk`, decoupled the timing-based KASLR leak (`rdtscp` SIGILL), and manually dumped/patched the Google `target_db` to bypass target OS validation.
* **Dynamic Race Tuning:** Recalibrated the `close-vs-close` thread-racing timing constants to accommodate the high-latency QEMU virtualization overhead, achieving a ~99% reliable Use-After-Free (UAF) trigger.
* **Offset Calibration:** Used GDB to reverse engineer the shifted `task_struct` offsets specific to our custom `vmlinux` binary (e.g., `comm`, `files`), achieving a stable Arbitrary Address Read (AAR).

## 2. Environment Analysis Performed
* **Reproducibility Audit:** Performed a static analysis of the repository to determine if another engineer could recreate the Tier 1 environment. 
* **Findings:** While the kernel and rootfs compilation is fully automated via `scripts/setup-tier1.sh`, the exploit compilation is heavily undocumented and relies on manual source-code patches.

## 3. Documentation & Verification Completed
* Authored `ENVIRONMENT_CONSTANTS.md` mapping custom struct offsets.
* Updated `logbook.md` with detailed debug tracking and root-cause analysis of crash states.
* Generated `REPRODUCIBILITY_REPORT.md` and a comprehensive `ENVIRONMENT_REBUILD_GUIDE.md` to ensure the work is entirely preserved.
* Established the `tier1.5-investigation` Git branch to safely isolate experimental baseline testing, keeping `main` stable.

## 4. Current Blocker
The exploit successfully triggers the UAF, corrupts the slab, achieves AAR, and attempts the final RIP hijack via the `file->f_op->poll` indirect call. 
However, the payload panics with `#PF: supervisor write access in kernel mode` at `__x86_indirect_call_thunk_rdi+0x5`. 

This is due to a misinterpretation of an AT&T syntax ROP gadget. The chosen gadget `mov %rdi,(%rsp)` actually performs a memory write to `[rsp]`, violating supervisor protections, rather than acting as a stack pivot (`mov rsp, rdi`).

## 5. What Remains to be Investigated
1. **Gadget Hunting:** We must use `ROPgadget` on our specific `vmlinux` binary to locate a true stack pivot (e.g., `push rdi; pop rsp; ret` or `xchg rsp, rdi; ret`).
2. **Exploit Update:** Update `PIVOT1` inside `exploit.cpp` with the new gadget offset to achieve arbitrary code execution and the final root shell.
3. **Automation:** Create Git patch files (`.patch`) for the manual exploit modifications so that `setup-tier1.sh` can build the exploit completely hands-free.
4. **Tiers 2 & 3:** Once Tier 1 is fully completed and reproducible, proceed to cross-compilation for Android (Tier 2) and SELinux confinement analysis (Tier 3).
