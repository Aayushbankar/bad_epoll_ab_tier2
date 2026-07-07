# Tier 1.5 – Environment Recreation

## Objective
The purpose of Tier 1.5 is to establish a known-good baseline that matches the original KernelCTF research environment. This allows us to distinguish between fundamental logic failures in the exploit and simple environmental mismatches.

## Original KernelCTF Environment
* **Kernel Version:** 6.1.x / 6.12.x LTS (specifically tailored with KCTF patches).
* **Kernel Configuration:** Google's proprietary `.config` enabling specific mitigations and modifying core data structures.
* **Compiler:** Clang/LLVM (instead of GCC, radically altering `.text` gadget availability and stack layouts).
* **QEMU / Virtualization:** KVM on high-performance Google Cloud nodes with full CPU passthrough.
* **Userspace:** Custom Debian-based minimal rootfs.
* **Mitigations:** KASLR, SMEP, SMAP, KPTI, restricted `dmesg`, disabled `bpf` for unprivileged users.

## Reproducibility Analysis

### What can be realistically recreated
* **The Kernel Binary & Image:** We can use the exact `bzImage` and `vmlinux` provided by the original author or by downloading the KernelCTF target database.
* **The Configuration:** The exact `.config` is available.
* **The File System:** The original `initramfs` or `rootfs` can be utilized.
* **The Exploit Code:** The exact hardcoded offsets and ROP chains provided in the public PoC.

### What cannot be reproduced exactly
* **CPU Timing & Hypervisor Overheads:** We are running locally on QEMU, likely nested. System call overhead, interrupt latency, and cache-line bounce timings will differ significantly from a GCP bare-metal instance.
* **Hardware Specifics:** The cache architecture (L1/L2/L3 size and sharing) of our local CPU vs the server CPU.

### Acceptable Differences
* **Slight Timing Variations:** These can be mitigated by keeping the dynamic timing calibration we developed in Tier 1 (`race_close_intr_threshold`).
* **KASLR Bypass Adjustments:** If `rdtscp` remains unavailable, hardcoding the KASLR base (when booting with `nokaslr`) is an acceptable difference because breaking KASLR is well-understood and tangential to the core epoll vulnerability.

### Differences Likely to Break the PoC
* **Any change to `.config`:** Modifying even a single configuration flag can shift struct padding, breaking Arbitrary Address Read/Write offsets.
* **Compiler Changes:** Recompiling with GCC instead of Clang will destroy the ROP/JOP chain and thunk layout.

## Comparison Table

| Component | Original KernelCTF Environment | Our Tier 1 Environment | Proposed Tier 1.5 Environment |
| :--- | :--- | :--- | :--- |
| **Kernel Source** | LTS 6.12.x + KCTF Patches | Custom upstream 6.12.67 | Exact KCTF `bzImage` release |
| **Compiler** | Clang/LLVM | GCC 12/13 | Pre-compiled (Clang) |
| **Config Layout** | KernelCTF custom | `make defconfig` + tweaks | KernelCTF exact `.config` |
| **QEMU CPU** | Full Passthrough (Host) | `kvm64` (default) | `-cpu host` (if possible) |
| **KASLR Leak** | `rdtscp` timing | Disabled (SIGILL) | Disabled or `-cpu host` + `rdtscp` |
| **Struct Offsets** | Original database | Manually reversed (shifted) | Original database |
| **Stack Pivots** | Original JOP gadgets | Custom `mov rsp, rdi` | Original JOP gadgets |
