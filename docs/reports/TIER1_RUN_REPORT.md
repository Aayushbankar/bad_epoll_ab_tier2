# Tier 1 Linux QEMU Run Report & Analysis

This document compiles the findings, execution details, failures, and structural alignments identified during the initial runs of the CVE-2026-46242 exploit on the custom compiled guest kernel `6.12.67` LTS.

---

## 1. What Worked (Successful Stages)

### A. Dynamic Timing Calibration
* **Behavior:** The racer thread initially hung in a timing calibration loop due to `0 interrupts` being detected under QEMU nested virtualization.
* **Resolution:** We implemented an adaptive fallback in `race_setup` (`exploit.cpp`) to measure standard close cycles and dynamically calibrate the threshold `race_close_intr_threshold`.
* **Result:** The race was won cleanly:
  ```text
  [+] race won: retries=444356 launch=2786 cc_retries=1
  [*] cross-cache...
  ```

### B. UAF & Cross-Cache Reclaim
* **Behavior:** The exploit successfully triggered false sharing and timerfd interrupt-based race window stretching to force the Use-After-Free (UAF) condition on `struct eventpoll`.
* **Result:** Reclaiming the victim slab as a pipe buffer page was successful, allowing us to forge a fake `struct file` and transition to the AAR stage.

### C. Race Timing Calibration & Timeout in QEMU
* **Failure:** The exploit successfully bypassed the `rdtscp` issue and successfully calibrated the `close()` interrupt threshold, but then repeatedly timed out after 300 seconds without winning the race (`race_retries=14313 cc_retries=1`). In a QEMU VM, syscalls and context switches are much slower, meaning the false-sharing window and the pre-computed `RACE_AHEAD_HI` constants were too narrow to land the timer interrupt correctly.
* **Resolution:** 
  - Reduced `RACE_DUP_CLOSE_ITERS` from 250 to 20 to adjust the false-sharing cache line bounce for the slower QEMU environment.
  - Increased `RACE_AHEAD_HI` from 4000 to 10000 and the step size to 500 to scan a much wider window for the timer interrupt.
  - Increased `EXPLOIT_DURATION_SEC` from 300s to 600s (10 minutes) to give the slower VM enough time to hit the race.

### D. Constrained AAR Page Fault (Struct Offset Mismatch)
* **Failure:** The exploit successfully won the race and reclaimed the pipe page, forging the fake `struct file`. However, when `ep_show_fdinfo()` triggered the AAR via the forged `f_inode` offset, the kernel panicked with `#PF: supervisor read access in kernel mode` at address `0x0000000000000010`. 
* **Cause:** The exploit hardcoded `task_struct` field offsets based on the original kernelCTF database (`target_db.kxdb`). Our custom QEMU kernel has a different configuration, meaning fields like `comm` shifted from `0x788` (1928) to `0x730` (1840). Consequently, the exploit computed the wrong base pointer for `i_sb` and `i_ino`, causing `ep_show_fdinfo` to dereference unmapped memory.
* **Resolution:** Used GDB on `vmlinux` to extract the correct field offsets for the custom kernel. Updated `exploit.cpp` with the true offsets:
  - `TASK_STRUCT_OFFS_COMM`: 1840
  - `TASK_STRUCT_OFFS_SAS_SS_SP`: 1984
  - `TASK_STRUCT_OFFS_FILES`: 1896
  - `TASK_STRUCT_OFFS_CHILDREN_NEXT`: 1384
  - `TASK_STRUCT_OFFS_CHILDREN_PREV`: 1392
  - `TASK_STRUCT_OFFS_SIBLING_NEXT`: 1400
  - `TASK_STRUCT_OFFS_SIBLING_PREV`: 1408

---

## 2. What Failed (Debugging & Crashes)

### A. KASLR Timing Leak
* **Failure:** Gruss timing leak side-channel prefetches failed to resolve the KASLR base address under QEMU. Furthermore, executing `leak_kaslr_base` inside a QEMU guest configured with the default `kvm64` CPU profile triggered a SIGILL (Illegal Instruction) because `kvm64` lacks support for the `rdtscp` instruction. Additionally, helper functions `rdtsc_begin()` and `rdtsc_end()` inside `exploit.cpp` used `rdtscp`, causing a SIGILL immediately after KASLR bypass during race timing calibration.
* **Resolution:** Bypassed the KASLR timing leak measurement code entirely in `exploit.cpp`, defaulting directly to the static base `0xffffffff81000000` (which matches the guest VM booting with the `nokaslr` parameter). Replaced the `rdtscp` instructions in `rdtsc_begin()` and `rdtsc_end()` with standard, universally supported `rdtsc` + `lfence` instruction sequences.

### B. Target Database Auto-Detect
* **Failure:** The exploit failed to resolve target properties from `/proc/version` checks.
* **Resolution:** Modified the initialization in `exploit.cpp` to manually load target `"kernelctf", "lts-6.12.67"` configuration.

### C. Arbitrary Address Read (AAR) Page Fault
* **Failure:** During the constrained AAR validation, the guest kernel crashed with a supervisor read page fault inside `ep_show_fdinfo`.
  ```text
  [  231.310961] BUG: unable to handle page fault for address: ffffffff8440d830
  [  231.313475] RIP: 0010:ep_show_fdinfo+0x4c/0x80
  ```
* **Analysis:** The crash occurred because the exploit was using the hardcoded default `init_task` offset (`0x340d0c0`) from the official kernelctf build. In the custom-compiled kernel, the layout shifted, making the calculated target pointer point to unmapped memory.

---

## 3. Discovered Offsets & Layout Mapping

To align the exploit with the custom-compiled kernel, we extracted the following layout metrics:

### A. Kernel Symbols (from `System.map`)
| Symbol Name | Offset in Custom Kernel | Default (Official) |
|-------------|-------------------------|--------------------|
| `init_task` | `0x1c0c940` | `0x340d0c0` |
| `vmemmap_base` | `0x1a461e8` | `0x292d788` |
| `page_offset_base` | `0x1a461f8` | `0x292d798` |

### B. ROP/JOP Stack Pivots (from `vmlinux` Disassembly)
We scanned the `.text` section of `vmlinux` using semantic analysis. We identified a direct stack-pointer hijack gadget generated by the compiler's thunk handling:
* **Selected Gadget:** `mov rsp, rdi`
* **Virtual Address:** `0xffffffff820d6525`
* **Kernel Offset:** `0x10d6525`
* **Disassembly:**
  ```assembly
  nop ; nop ; nop ; nop ; call <__x86_indirect_call_thunk_rdi+0xf> ; int3 ; mov QWORD PTR [rsp], rdi ; ret
  ```

---

## 4. Final Realignment Action Plan

To run the exploit successfully without crashes:
1. **Patch `exploit.cpp` macros** with the custom offsets:
   - `INIT_TASK = 0x1c0c940`
   - `VMEMMAP_BASE = 0x1a461e8`
   - `PAGE_OFFSET_BASE = 0x1a461f8`
2. **Replace the 4-gadget JOP pivot chain** with the single direct pivot `mov rsp, rdi` at offset `0x10d6525`.
3. **Compile, package the initramfs, and run** inside the QEMU guest.
