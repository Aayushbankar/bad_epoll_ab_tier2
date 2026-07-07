# Reproducibility Report
**Target Phase:** Tier 1 (Linux VM via QEMU)
**Date:** 2026-07-07
**Analysis Type:** Static Repository Analysis

## Executive Summary
This report analyzes whether the "yesterday baseline" (the state of the repository immediately prior to the Tier 1.5 tests) can be successfully recreated from scratch on an empty Linux machine using only the provided scripts and documentation.

**Conclusion:** The environment setup (kernel compilation and rootfs generation) is highly reproducible thanks to `scripts/setup-tier1.sh`. However, the **exploit compilation and payload generation** process is currently non-reproducible through automation. An engineer would need to manually intervene, read `logbook.md`, and apply source-code patches to the exploit to successfully build and run it.

---

## Environment Inventory
- **Host OS:** Assumed Fedora (based on `logbook.md` and package names).
- **Target Kernel:** `6.12.67` LTS
- **Virtualization:** QEMU (`kvm64` CPU profile)
- **Rootfs:** Minimal `initramfs` powered by statically linked `busybox`.

## Dependency Inventory
`scripts/setup-tier1.sh` correctly identifies and checks for:
- `gcc`, `g++`, `make`, `qemu-system-x86_64`, `cpio`, `flex`, `bison`

**Missing/Implicit Dependencies:**
Based on `logbook.md`, the following host dependencies are required to compile the exploit and `libxdk` but are **missing from automated setup scripts**:
- `cmake`
- `keyutils-libs-devel`
- `libstdc++-static`
- `glibc-static`

## Build Process & Reproduction Status

### 1. Kernel and Rootfs Compilation
- **Status:** **Highly Reproducible**
- **Process:** Running `./setup-tier1.sh` automatically downloads the kernel, configures it (`CONFIG_EPOLL`, `CONFIG_DEBUG_INFO`), compiles `bzImage`, and creates `initramfs.cpio` with `busybox` and an `init` script.
- **Issues:** None. The script is self-contained.

### 2. Exploit Compilation
- **Status:** **Manual Intervention Required (Not Automated)**
- **Process:** The `README.md` suggests a simple `g++ -static` command to build the exploit. This is inaccurate. 
- **Issues:** 
  1. The exploit relies on `libxdk`, which requires `cmake` and `make`.
  2. Compiling on a modern host (GCC 16) causes severe C++ standard template errors in `PayloadBuilder.cpp`. 
  3. The `setup-tier1.sh` does not apply the necessary code patches to fix `<functional>` headers and pointer reference wrappers.
  4. The exploit's `kxdb.AutoDetectTarget()` fails on custom kernels, requiring manual source-code patching to force-load the target.
  5. The `rdtscp` instruction causes a `SIGILL` under QEMU's `kvm64` profile, requiring manual removal of KASLR leak code.

### 3. Execution & Layout Calibration
- **Status:** **Requires Manual Tuning**
- **Process:** The exact layout of `task_struct` on the locally compiled kernel differs from the Google KernelCTF database. 
- **Issues:** While `ENVIRONMENT_CONSTANTS.md` documents the correct offsets (e.g., `comm` at 1840, `files` at 1896), there is no script that automatically applies these offsets to the exploit source. The engineer must manually edit `exploit.cpp` before compilation.

## Missing Information & Known Issues
- **No Patch Files:** All the source code fixes mentioned in `logbook.md` (fixing `libxdk`, bypassing KASLR, fixing `task_struct` offsets) were done live by the user/AI pair. There are no `.patch` files committed to the repository to re-apply these changes to a fresh clone of the exploit repo.
- **Stack Pivot Failure:** The environment crashes at the final step (`__x86_indirect_call_thunk_rdi+0x5`) due to an incorrect AT&T syntax interpretation of a ROP gadget. This remains unfixed in the baseline.

## Confidence Level
- **Environment Generation (Kernel/QEMU):** 100% confidence.
- **Exploit Build & Execution:** 20% confidence for automated reproduction; 80% confidence if the engineer meticulously follows `logbook.md` to recreate the manual code edits.

## Lessons Learned
To make this repository perfectly reproducible, we must:
1. Add `cmake` and static libc/libc++ to the dependency checker in `setup-tier1.sh`.
2. Generate Git `.patch` files for all modifications made to `exploit.cpp` and `libxdk`, and have `setup-tier1.sh` automatically apply them using `git apply` or `patch`.
