# Tier 2 Engineering Roadmap

## Phase 1: Environment & Tooling Scaffold
**Goal**: Establish a reproducible local development and debugging environment targeting an Android ARM64 kernel.
- **Deliverables**: Working Android Emulator instance, cross-compilation toolchain, GDB-multiarch connected to kernel debugger.
- **Expected Blockers**: Identifying the correct vulnerable GKI build; extracting `vmlinux` with DWARF symbols from AOSP.
- **Evidence Required**: Screenshot of GDB halting the Android kernel; `uname -a` output showing the correct kernel version.
- **Exit Criteria**: A "Hello World" cross-compiled C binary executes successfully in the emulator shell, and kernel execution can be paused/inspected via GDB.
- **Risk Level**: Low (Standard engineering setup)

## Phase 2: Vulnerability Verification on ARM64
**Goal**: Prove the core `close-vs-close` UAF trigger functions on the Android kernel.
- **Deliverables**: C++ exploit binary that triggers the target memory corruption without relying on subsequent payload execution.
- **Expected Blockers**: Different `epoll` SLUB cache sizes or alignment requirements; timing shifts on emulated ARM cores.
- **Evidence Required**: Kernel panic log (`dmesg`) showing a Use-After-Free or KASAN report at the exact expected instruction.
- **Exit Criteria**: The vulnerability can be triggered deterministically >= 80% of the time.
- **Risk Level**: Medium

## Phase 3: Primitive Reconstruction
**Goal**: Achieve stable Arbitrary Address Read (AAR) and Arbitrary Address Write (AAW).
- **Deliverables**: Updated `MemoryData` classes in `libxdk` tailored for ARM64 heap grooming.
- **Expected Blockers**: `kmalloc` vs `kmem_cache` alignment discrepancies; cross-cache spraying constraints specific to GKI.
- **Evidence Required**: Logs showing successful KASLR bypass (leaked kernel base address).
- **Exit Criteria**: The exploit can read the `init_task` structure from physical memory without crashing.
- **Risk Level**: High

## Phase 4: Mitigation Bypass (PAC/BTI)
**Goal**: Map out an execution path that circumvents hardware control flow integrity.
- **Deliverables**: A strategy document detailing how PAC and BTI will be defeated (e.g., data-only attacks, un-PAC'd function pointer hijacking, or JOP chain construction).
- **Expected Blockers**: PAC drastically limits the viability of traditional `f_op->poll` hijacking used in Tier 1.
- **Evidence Required**: Identification of an exploitable function pointer that lacks strict PAC/BTI enforcement, or a verified data-only overwrite target (like `selinux_state`).
- **Exit Criteria**: Theoretical execution path is mapped and validated in GDB.
- **Risk Level**: Critical (Highest engineering risk)

## Phase 5: Privilege Escalation
**Goal**: Obtain `UID 0` and unconfined SELinux context.
- **Deliverables**: The final payload implementing `commit_creds` or equivalent, and SELinux disabling logic.
- **Expected Blockers**: Unpredictable kernel stack alignment upon returning to user space (`EL1` to `EL0` transition).
- **Evidence Required**: Terminal screenshot of `# id` returning `root`.
- **Exit Criteria**: A persistent, interactive root shell is spawned from the exploit process.
- **Risk Level**: High

## Phase 6: Stability & Documentation
**Goal**: Finalize the exploit for portfolio demonstration and peer review.
- **Deliverables**: Cleanly commented source code, final engineering write-up.
- **Expected Blockers**: Background threads crashing after the primary exploit completes.
- **Evidence Required**: Video recording of the exploit running reliably from a fresh boot.
- **Exit Criteria**: Repository is frozen and documented.
- **Risk Level**: Low
