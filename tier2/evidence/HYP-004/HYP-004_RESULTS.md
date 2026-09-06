# HYP-004: Alternative Page Reclaim when DMABUF_HEAPS_SYSTEM is Absent — Results

## Executive Summary
- **Objective**: Identify and evaluate viable mechanisms to allocate order-0 pages from the buddy allocator that can catch pages freed by `struct file` drain (EXP-027), specifically focusing on whether the memory can be mapped and edited in-place from userspace.
- **Result**:
  1. **Kernel Rebuild with `CONFIG_DMABUF_HEAPS_SYSTEM=y`**: **FEASIBLE**. The source code `drivers/dma-buf/heaps/system_heap.c` is present and compiles cleanly into `system_heap.o`. On Pixel 10 (as cited in reference research), this driver is built-in or loaded as a vendor module.
  2. **`pipe_buffer` Page Allocation**: **PARTIAL**. Writing data to a pipe allocates order-0 pages from the buddy allocator (`alloc_page(GFP_HIGHUSER | __GFP_ACCOUNT)`). However, pipes do not implement `mmap()`; userspace cannot perform in-place live edits on pages held in a pipe buffer.
  3. **`memfd_create()` / `tmpfs`**: **VIABLE & UNPRIVILEGED**. Provides order-0 buddy pages that can be mapped via `mmap(..., MAP_SHARED)`. Userspace retains a writable pointer, enabling continuous in-place edits to fake file fields (`f_op`, `f_lock`, `private_data`).

---

## Controlled QEMU Runtime Audit
Quoting raw serial log from `tier2/evidence/HYP-004/HYP-004_raw_serial.log` (lines 249-265):

```
=== HYP-004: Alternative Page Reclaim Audit ===
[+] memfd_create: order-0 buddy page allocated and live-editable via mmap at 0x7f8c9de000
[+] pipe write: allocated order-0 page from buddy allocator (4096 bytes)
[*] pipe mmap rejected as expected (errno=13: Permission denied) -> Not live-editable
================================================================
HYP-004 CONCLUSION:
1. CONFIG_DMABUF_HEAPS_SYSTEM=y can be rebuilt into GKI (system_heap.o compiles).
2. memfd_create provides unprivileged order-0 buddy pages that are live-editable via MAP_SHARED.
3. pipe_buffer provides order-0 buddy pages but cannot be mmap'd (no live in-place edits).
================================================================
```

---

## Comparison of Buddy Reclaim Targets

| Mechanism | Kernel Source / Subsystem | Allocator Call | Order | Unprivileged Accessible? | Mmapable (Live-Editable)? | Viable for Fake File? |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **DMA-BUF System Heap** | `drivers/dma-buf/heaps/system_heap.c` | `alloc_pages(GFP_HIGHUSER, 0)` | 0 | Yes (via `/dev/dma_heap/system` when enabled) | **YES** (`dma_buf_mmap`) | **YES** (Standard Pixel 10 target) |
| **`memfd_create` (tmpfs)** | `mm/shmem.c` | `alloc_pages(GFP_HIGHUSER, 0)` | 0 | **YES** (Standard POSIX syscall) | **YES** (`mmap(MAP_SHARED)`) | **YES** (Standard Linux target) |
| **`pipe_buffer`** | `fs/pipe.c:pipe_write` | `alloc_page(GFP_HIGHUSER \| __GFP_ACCOUNT)` | 0 | **YES** (`pipe()`) | **NO** (No `f_op->mmap`) | **NO** (Cannot live-edit fake `f_op`) |
| **`ashmem`** | `drivers/staging/android/ashmem.c` | `shmem_alloc_page()` | 0 | Conditional (Needs Android `ueventd` chmod) | **YES** (`mmap(MAP_SHARED)`) | **YES** (On full Android userspace) |

---

## Conclusion
If testing without modifying the kernel `.config`, `memfd_create()` is the primary viable alternative: it allocates order-0 pages from the buddy allocator and provides an unprivileged, `mmap`'d live-editable memory buffer. If replicating the Pixel 10 environment verbatim, rebuilding the GKI image with `CONFIG_DMABUF_HEAPS_SYSTEM=y` enables `/dev/dma_heap/system`.
