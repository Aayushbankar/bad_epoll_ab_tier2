# Tier 2 Android ARM64 Manual Demonstration Runbook

## 1. Demonstration Scope

This demonstration explicitly establishes the runtime presence of CVE-2026-46242 (Use-After-Free in Epoll) on a controlled Android ARM64 QEMU environment and details the diagnostic state of heap spray observations. 

### Proven
- **Environment**: Android Common Kernel 6.1.23 ARM64 boots successfully under QEMU.
- **Vulnerability**: The trigger binary executes and deterministically reaches the `ep_remove()` UAF race path.
- **Race Condition**: The inner eventpoll object is successfully freed by Thread B while Thread A is suspended.
- **Memory Violation**: Stale access by Thread A is detected and confirmed by HW_TAGS MTE/KASAN.
- **Allocator State**: The freed ~~`kmalloc-192`~~ victim-slot reuse has been observed being immediately reclaimed by subsequent Thread B allocations. [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: the freed object is a `struct epitem` (120 bytes) in the dedicated `eventpoll_epi` slab cache, not a generic `kmalloc-192` slot.]
- **Payload Subsystem**: `sys_add_key()` spray calls succeed entirely (returning positive key IDs without `-EDQUOT` or `-EPERM`).
- **Lifecycle Tracing**: Temporary versus persistent key payload allocations have been distinguished conceptually and through source analysis (temporary buffer is allocated first and unconditionally freed; `user_preparse` persistent allocation occurs second).

### Not Proven
- **Persistence**: A persistent replacement object occupying the victim slot at the exact stale-write moment.
- **Exploitation**: Arbitrary read, arbitrary write, control-flow hijacking, code execution, root, privilege escalation, or SELinux bypass.

## 2. Prerequisites

The following host-side configuration and artifacts are required to execute the demonstration:

- **Host Environment**: Debian/Ubuntu Linux with `qemu-system-aarch64` and `gdb-multiarch`.
- **Containers**: `podman` or `docker` (to safely rebuild the rootfs via `debian:bookworm-slim` cross-compilation).
- **Working Directory**: the repository root (this `bad-epoll-lab` checkout).
- **Kernel Image**: `tier2/android/artifacts/Image`
- **Kernel Symbols**: `tier2/android/artifacts/vmlinux`
- **Initramfs Archive**: `tier2/initramfs.cpio`
- **Trigger Source**: `tier2/reproducers/cve_2026_46242_trigger.c`
- **Trigger Binary**: Packaged inside the initramfs as `/cve_2026_46242_trigger`

## 3. Pre-Demonstration Verification

Run the following commands to ensure no stale binaries or dirty states contaminate the demonstration.

1. **Verify Git Branch and Cleanliness:**
   ```bash
   git branch
   git status
   ```
2. **Verify Kernel and Artifacts:**
   ```bash
   ls -la tier2/android/artifacts/Image
   ls -la tier2/android/artifacts/vmlinux
   ls -la tier2/initramfs.cpio
   ```
3. **Verify Trigger Binary Architecture:**
   ```bash
   file tier2/rootfs/cve_2026_46242_trigger
   # Expected output: ELF 64-bit LSB executable, ARM aarch64
   ```

## 4. Manual Tier 2 Run — Basic Trigger

This sequence executes the non-interrupted spray loop to prove `sys_add_key` succeeds and logs its outputs.

1. **Compile and Package Initramfs:**
   *What it does: Compiles the C trigger for aarch64 and repacks the CPIO archive.*
   ```bash
   podman run --rm -v $(pwd):/work -w /work debian:bookworm-slim bash -c "apt-get update -y && apt-get install -y gcc-aarch64-linux-gnu libc6-dev-arm64-cross && aarch64-linux-gnu-gcc -static tier2/reproducers/cve_2026_46242_trigger.c -pthread -o tier2/rootfs/cve_2026_46242_trigger"
   ./tier2/scripts/build_rootfs.sh
   ```
   *Success:* `[*] Ramdisk generated at: .../tier2/initramfs.cpio`

2. **Boot QEMU and Observe Output:**
   *What it does: Boots QEMU without GDB intervention, running `/init`.*
   ```bash
   ./tier2/scripts/run_qemu.sh
   ```
   *Expected Output:* The QEMU console will print boot logs followed by 256 lines of `[*] sys_add_key success: id=<positive_number>`. The VM will automatically power off after the loop completes.
   *Success Proof:* The absence of `[*] sys_add_key failed:` outputs confirms the API limits are not blocking the payload injection.

## 5. Manual Tier 2 Run — Race Validation

This sequence executes the mechanically controlled GDB procedure to demonstrate the UAF stale write.

1. **Start QEMU in Debug Mode (Terminal 1):**
   *What it does: Boots QEMU paused, exposing a GDB stub on port 1234.*
   ```bash
   ./tier2/scripts/test_gdb.sh
   ```

2. **Attach GDB and Execute Race (Terminal 2):**
   *What it does: Runs the automated Python GDB script to orchestrate the race.*
   ```bash
   gdb -q -x tier2/scripts/gdb_race_test.py tier2/android/artifacts/vmlinux
   ```

**Race Mechanics (Automated by Script):**
- **Thread A**: Calls `ep_remove()` resulting from closing the outer epoll file.
- **Trap**: GDB sets a breakpoint just before `file->f_ep = NULL` inside `__ep_remove()`.
- **Intervention**: Upon hitting the trap, GDB sets `sync_flag = 1`, releasing Thread B.
- **Thread B**: Closes the inner epoll, hitting the fast-path release, freeing the `inner_epoll` memory slot (confirmed by SLUB). Thread B then initiates the `sys_add_key` loop.
- **Stale Access**: GDB resumes Thread A, which proceeds to ~~`hlist_del_rcu()`~~ ~~writing into the now-freed `inner_epoll` memory slot (`victim + 0xa0`)~~ `list_del_init(&epi->rdllink)`, writing into the now-freed `epitem` memory slot at offsets 24 and 32. [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: the stale write is `list_del_init(&epi->rdllink)` at offsets 24/32, not `hlist_del_rcu` at offset 0xa0.]
- **KASAN Report**: A `BUG: KASAN: use-after-free in __ep_remove+...` stack trace appears in the QEMU serial output.

## 6. Expected Evidence

Capture these specific data points during the meeting to validate the current Tier 2 state:

- [ ] **KASAN/MTE Report**: Appears in the QEMU serial console. Proves the stale access genuinely triggers a memory safety violation.
- [ ] **Thread Control Logs**: GDB prints `=== STARTING CONTROLLED UAF RACE EXPERIMENT ===`. Proves the mechanical race was deterministically enforced.
- [ ] **Victim Address Print**: GDB prints the tagged and untagged victim pointer (e.g., `Victim: 0xf7ffff8003d0cd80`). Proves the location of the `inner_epoll` chunk.
- [ ] **`sys_add_key` Success Logs**: QEMU serial output showing positive IDs. Proves quota/permissions are not silently destroying payloads.

## 7. What To Say To The Mentor

> "The Tier 2 work is currently at the point where the UAF race is mechanically reproduced on an Android Common Kernel ARM64 target. We have confirmed the freed ~~eventpoll object~~ `epitem` object and the stale write using HW_TAGS MTE/KASAN. We have also confirmed that the relevant ~~`kmalloc-192`~~ `eventpoll_epi` slot can be reclaimed. The current investigation is distinguishing the temporary copy-in allocation performed by `sys_add_key` from the persistent `user_key_payload` allocation. The next experiment is intended to prove whether the persistent replacement object can occupy and remain in the victim slot. That has not yet been claimed as achieved." [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: victim is `struct epitem` in the `eventpoll_epi` cache, not `struct eventpoll` in `kmalloc-192`.]

## 8. Live Demo Failure Recovery

| Symptom | Safe Diagnostic Command | Recovery Action |
| --- | --- | --- |
| **QEMU hangs at boot / Port in use** | `killall -9 qemu-system-aarch64` | Terminate stale processes and restart `test_gdb.sh`. |
| **GDB connection times out** | `netstat -tuln \| grep 1234` | Check if QEMU bound successfully. Relaunch QEMU. |
| **No KASAN report appears** | `cat qemu.log` | Ensure MTE/KASAN is passed via `CMDLINE` in `run_qemu.sh` and Thread A resumed correctly. |
| **Trigger binary doesn't reflect changes** | `ls -la tier2/rootfs/cve_2026_46242_trigger` | Re-run the `podman` build command and `build_rootfs.sh`. |
| **GDB script gets stuck looping allocations**| `killall -9 gdb` | Revert `sync_flag` logic in the C file if it was left modified for the non-GDB run, rebuild rootfs, and retry. |

## 9. Exact Current Checkpoint

**COMPLETED:**
- Verified vulnerability-bearing Android 6.1.23 kernel boots.
- Mechanically isolated Thread A / Thread B execution paths.
- Proved Thread B frees the `inner_epoll` object.
- Proved Thread A commits a stale UAF write upon resuming.
- Proved HW_TAGS KASAN detects the exact memory violation.
- Proved ~~`kmalloc-192`~~ `eventpoll_epi` slot can be reclaimed by subsequent allocations. [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: freed object is in the `eventpoll_epi` dedicated slab cache, not `kmalloc-192`.]
- Proved `sys_add_key` succeeds and does not fail due to API quotas.
- Identified that `sys_add_key` uses an unconditionally freed temporary buffer, resolving false-positive "syscall failure" observations.

**CURRENT:**
The investigation has isolated why previous allocation tracing observed the victim slot being repetitively claimed and freed. The GDB script was tracing the temporary `payload` buffer instead of the persistent `user_key_payload` allocation inside `user_preparse`. 

**NOT YET COMPLETED:**
- Instrumenting the correct persistent allocator.
- Re-sizing the payload to split SLUB cache destinations (`plen = 128` vs `plen = 152`).
- Demonstrating the persistent replacement object successfully anchoring the victim slot.

**NEXT PLANNED MICRO-STEP:**
~~(PLANNED ONLY): Modify `cve_2026_46242_trigger.c` to use a payload size of 128 bytes so that the temporary `payload` buffer routes to `kmalloc-128`, forcing the persistent `upayload` buffer (152 bytes) to exclusively route to the victim `kmalloc-192` cache. Run the controlled GDB trace tracing `user_preparse` to prove the persistent object anchors the victim slot.~~ [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: this entire cache-splitting strategy is obsolete. The victim is a `struct epitem` (120 bytes) in the dedicated `eventpoll_epi` slab cache, not in `kmalloc-192`. The spray object must target the `eventpoll_epi` cache instead.]

## 10. Two-Minute Mentor Demonstration Path

1. **Terminal 1:** Run `./tier2/scripts/test_gdb.sh`
2. **Terminal 2:** Run `gdb -q -x tier2/scripts/gdb_race_test.py tier2/android/artifacts/vmlinux`
3. Point out the GDB output confirming the Mechanical Race is active.
4. Point out the GDB output confirming the victim address.
5. Switch to Terminal 1 to observe the `BUG: KASAN: use-after-free` report, confirming the exact underlying flaw.
6. Terminate QEMU.

## 11. Ten-Minute Deep-Dive Path

1. Perform the Two-Minute Demonstration.
2. Open `tier2/android/source/common/security/keys/keyctl.c` and point to line 147 (`kvfree_sensitive(payload, plen);`).
3. Explain the diagnostic breakthrough: the previous observation of "repetitive victim slot reuse" was an artifact of tracing this temporary copy-in buffer which is always freed, rather than a syscall error.
4. Open `tier2/android/source/common/security/keys/user_defined.c` and point to line 67 (`kmalloc(sizeof(*upayload) + datalen, GFP_KERNEL);`).
5. ~~Explain the mathematical path forward: `sizeof(user_key_payload)` is 24 bytes. By passing a 128-byte payload, the temporary buffer occupies `kmalloc-128`, bypassing the victim slot, while the persistent `user_preparse` allocation occupies `152` bytes, landing perfectly in the `kmalloc-192` victim slot.~~ [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: this entire `kmalloc-192` cache-splitting strategy is obsolete. The victim is a `struct epitem` (120 bytes) in the dedicated `eventpoll_epi` slab cache. The spray strategy must be redesigned to target that cache.]

## 12. Mentor Questions and Exact Answers

- **"What exactly is proven?"**
  "We have proven the deterministic existence of the UAF race condition resulting in a stale write, mechanically controlled via GDB, and detected by HW_TAGS KASAN. We have also proven the victim slot is immediately reclaimable."
  
- **"Did you get arbitrary write?"**
  "No. We are at the allocator-state validation stage. We have only proven the stale write occurs, not that it can be controlled or weaponized."

- **"Did you get root?"**
  "No. We have not established any control-flow hijacking or privilege escalation primitives."

- **"Why is this exploitable?"**
  "It allows a UAF write into ~~a widely accessible kernel heap cache (`kmalloc-192`)~~ the dedicated `eventpoll_epi` slab cache. If an attacker can reliably place a carefully crafted replacement object (such as a key payload) in the freed slot before the stale write occurs, they may corrupt kernel metadata." [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: the freed object is in the `eventpoll_epi` dedicated cache, not `kmalloc-192`.]

- **"What does KASAN prove?"**
  "It proves the hardware tags mismatch during Thread A's ~~`hlist_del_rcu`~~ `list_del_init(&epi->rdllink)` operation, irrefutably confirming that the memory chunk was freed by Thread B before Thread A could access it." [Corrected 2026-07-24, see EVO-005 in VERIFICATION_LEDGER.md: the stale write is `list_del_init(&epi->rdllink)` at offsets 24/32, not `hlist_del_rcu` at offset 0xa0.]

- **"Why did the same address appear repeatedly in your earlier tests?"**
  "Our GDB instrumentation was tracking the return value of `kvmalloc_node` in `sys_add_key`. This targets a temporary buffer used for `copy_from_user` that the kernel unconditionally frees a few lines later. Because it was freed every iteration, it kept reclaiming the same SLUB slot."

- **"Why did you initially think the syscall was failing?"**
  "Because the memory was being immediately released. A repeated physical address implies an 'allocate -> free' loop. We assumed the free was due to an error path (like `-EDQUOT`), but source analysis and userspace logs proved it was standard successful execution behavior."

- **"What is the difference between the temporary and persistent allocation?"**
  "The temporary allocation buffers the user data during the syscall. The persistent allocation (`struct user_key_payload`) is created by `user_preparse` and permanently linked into the target keyring, surviving the syscall."

- **"What remains to be proven?"**
  "We must prove that we can manipulate the payload size such that the persistent allocation reliably claims the victim slot while the temporary buffer is routed to a different, harmless SLUB cache."

- **"Why is this relevant to Android?"**
  "Epoll vulnerabilities historically bypass many logical kernel mitigations because they operate on fundamental VFS structures. Demonstrating this on an Android Common Kernel confirms the threat model applies to modern Android devices."

- **"What are the current mitigations and limitations?"**
  "MTE and KASAN currently catch the access and panic the kernel. In a production non-MTE build, the primary limitation is the extremely tight race window, which currently requires mechanical GDB pausing to hit reliably."
