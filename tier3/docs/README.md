# Tier 3 (Phase 3): Android 15 GKI Linux 6.6 Exploit Port

> **Target Kernel**: `android15-6.6-2025-10_r1`  
> **Commit Hash**: `3dff304da0a6c0edccc00413c1f03518377ea96d`  
> **Base Kernel**: Linux 6.6.102 GKI (Android 15)  
> **Build ID**: `ab14202157`  
> **Target Status**: **CONFIRMED NATIVELY VULNERABLE** (VER-070 / VER-071)  
> **Scope Distinction**: Targets `android15-6.6-2025-10_r1` specifically. Later respins (such as `_r32`) contain Google's cherry-picked fix and are NOT vulnerable.

---

## 1. Directory Structure

```text
tier3/
├── artifacts/
│   ├── Image (active kernel for QEMU)
│   ├── Image_certified (production certified boot Image)
│   ├── vmlinux (reconstructed ELF with 127,705 symbols)
│   ├── boot-6.6.img & boot-6.6-allsyms.img
│   └── certified_boot/ & debug_boot/
├── docs/
│   └── README.md
├── evidence/
│   ├── PHASE3_DAY1_SETUP_RESULTS.md (Day 1 execution results)
│   ├── ep_remove_disasm_6.6.102.txt (__ep_remove disassembly)
│   ├── qemu_serial_certified_6.6.102.log
│   └── qemu_serial_debug_6.6.102.log
├── rootfs/
│   ├── init (static musl init)
│   ├── harness (static musl test harness)
│   └── init.c
├── scripts/
│   ├── run_qemu.sh (QEMU launch script)
│   └── sanity_test_6_6.c (environment sanity test)
└── initramfs.cpio
```

---

## 2. Key Commands

```bash
# Compile harness & init
./tier2/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O2 -o tier3/rootfs/init tier3/rootfs/init.c
./tier2/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O2 -o tier3/rootfs/harness tier3/scripts/<HARNESS>.c

# Package initramfs
cd tier3/rootfs && find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio && cd ../..

# Launch QEMU (normal serial to file)
./tier3/scripts/run_qemu.sh

# Launch QEMU with GDB stub (:1234)
DEBUG=1 ./tier3/scripts/run_qemu.sh
```

---

## 3. Verified Architectural Facts (6.6.102 vs 6.1.23)

| Fact / Offset | GKI 6.1.23 | GKI 6.6.102 (r1) | Status |
|:---|:---|:---|:---|
| Native vulnerability | No (synthetic cherry-pick) | **YES (natively in-tree)** | VER-070 / VER-071 |
| Fix status in r1 | Absent | **ABSENT** (no `hlist_is_singular_node`, no `ep_remove` split, no `epi_fget` pin) | VER-070 / Day 1 |
| Fix status in r32 | N/A | **FIXED** (cherry-picked in r32) | VER-071 |
| `swaps_poll` write offset | `private_data + 0x60` (96) | **`private_data + 0x70` (112)** | Day 1 empirical disassembly |
| Linear map base | `0xffffff8000000000` | **`0xffffff8000000000`** | Day 1 verification |
| `filp` object size | 232 bytes (256 stride) | **232 bytes (264 stride)** | Day 1 verification |
