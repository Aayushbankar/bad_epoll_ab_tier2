# Android GKI Kernel Map

## 1. Ground Truth Parameters
- **Kernel String**: `Linux version 6.1.23-android14-4-00257-g7e35917775b8-ab9964412 (build-user@build-host)`
- **AOSP Branch**: `common-android14-6.1`
- **Commit Hash**: `7e35917775b8`
- **Build ID**: `9964412`

## 2. Memory Map & Layout
*(To be populated after GDB attachment and base inspection)*
- **Kernel Base (KASLR)**: `0xffffffc0xxxxxxx`
- **Module Base**: `0xffffffd0xxxxxxx`
- **vmemmap**: `0xfffffffexxxxxxx`
- **Direct Map (Phys)**: `0xffffff80xxxxxxx`

## 3. Exploit Offsets
*(To be populated in tier2/artifacts/offsets/)*
- `epitem->ffd`: TBD
- `f_op->poll`: TBD
- `task_struct->cred`: TBD

## 4. Verified Security Configurations (from /proc/config.gz equivalent)
- **PAC (Pointer Authentication)**: `CONFIG_ARM64_PTR_AUTH_KERNEL=y` (Enabled. Function pointers have cryptographic signatures).
- **BTI (Branch Target Identification)**: `CONFIG_ARM64_BTI_KERNEL=y` (Enabled. Indirect branches must land on specific instructions).
- **CFI (Control Flow Integrity)**: `CONFIG_CFI_CLANG=y` (Enabled. Forward-edge control flow is strictly validated by Clang).
- **SLUB Allocator**:
  - `CONFIG_SLAB_FREELIST_RANDOM=y`
  - `CONFIG_SLAB_FREELIST_HARDENED=y`
