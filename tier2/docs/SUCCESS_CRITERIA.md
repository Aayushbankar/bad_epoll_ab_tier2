# Success Criteria & Milestones

The Tier 2 project is divided into measurable stages. Each stage is strictly defined by an objective proof of completion.

## Stage 1: Android Environment Operational
- **Objective Proof**: A local Android Emulator (ARM64) successfully boots a known vulnerable GKI version, and `adb shell` is established.

## Stage 2: Kernel Sourced and Built
- **Objective Proof**: The `vmlinux` binary (with DWARF symbols) perfectly matching the emulator's kernel is extracted or compiled.

## Stage 3: Exploit Compiles for ARM64
- **Objective Proof**: The `libxdk` framework and Bad Epoll logic compile seamlessly using `aarch64-linux-gnu-g++` without architecture-specific syntax errors.

## Stage 4: Primitive Works
- **Objective Proof**: The exploit triggers the UAF race condition on the emulator, bypassing KASLR and successfully executing an Arbitrary Address Read (AAR) without a kernel panic.

## Stage 5: Target Structures Mapped
- **Objective Proof**: The `task_struct` and SELinux state variables are accurately located in memory using the rebuilt ARM64 offset database.

## Stage 6: Control Flow Hijack
- **Objective Proof**: A hardware breakpoint in `gdb` confirms the instruction pointer (`$pc`) has been successfully redirected to an exploit-controlled payload or JOP gadget, bypassing or accommodating PAC/BTI.

## Stage 7: Privilege Escalation
- **Objective Proof**: The payload successfully invokes `commit_creds` (or overwrites credentials in memory) and neutralizes SELinux enforcement.

## Stage 8: Root Shell
- **Objective Proof**: The exploit thread safely resumes userland execution, invoking `execve` to spawn a persistent root shell (`UID 0`) over ADB.

## Stage 9: Documentation Complete
- **Objective Proof**: A finalized Tier 2 engineering case study is published, accompanied by full evidence logs and reproducible source code.
