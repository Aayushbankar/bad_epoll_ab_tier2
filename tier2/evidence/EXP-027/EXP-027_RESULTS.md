# EXP-027: Page-Level struct file Cross-Cache Reclaim — Results

## Executive Summary
- **Objective**: Check `CONFIG_DMA_HEAP` / `CONFIG_DMABUF_HEAPS` availability and `/dev/dma_heap/system` accessibility from an unprivileged context on GKI 6.1.23. Test mass-freeing `struct file` (using fork-holding to bypass `RLIMIT_NOFILE`) to drain `filp` slab pages back to the buddy allocator.
- **Ground-Truth Signal**: `/sys/kernel/slab/filp/slabs` (`num_slabs` before, peak, and after drain).
- **Result**:
  1. **DMA-BUF Heaps Availability**:
     - `CONFIG_DMABUF_HEAPS=y`, `/sys/class/dma_heap` exists in sysfs.
     - However, `# CONFIG_DMABUF_HEAPS_SYSTEM is not set` in GKI 6.1.23 `.config`. Consequently, `/dev/dma_heap/system` is **not registered**.
     - `/dev/ashmem` is present (`CONFIG_ASHMEM=y`, misc minor 125). In this minimal test environment, access from UID 1000 yielded `errno=13: Permission denied` (requires Android userspace `ueventd` permission configuration).
  2. **filp Slab Drain to Buddy Allocator**: **CONFIRMED (99.55% efficiency)**.
     - Allocating 32,000 `struct file` objects across 40 forked processes expanded `filp` from 2 slabs to 2,002 slabs (+2,000 slabs, +32,000 objects).
     - Mass-freeing all 32,000 files dropped `filp` from 2,002 slabs to 11 slabs within 1 second.
     - **1,991 slabs (31,917 objects)** were drained directly back to the buddy allocator.

---

## DMA-BUF Heap & Device Node Audit
Quoting raw serial output from `tier2/evidence/EXP-027/EXP-027_raw_serial.log` (lines 253-263):

```
[*] Created /dev/ashmem with minor 125

--- Checking DMA-BUF Heaps & Allocator Nodes ---
[+] /sys/class/dma_heap exists. Enumerating devices:
    (no dma_heap devices registered in sysfs)
[-] /dev/dma_heap/system does not exist (CONFIG_DMABUF_HEAPS_SYSTEM is not set in GKI .config)
[+] /dev/ashmem exists (CONFIG_ASHMEM=y)

--- Testing Unprivileged Access (UID 1000) ---
[*] Dropped privileges: getuid()=1000, getgid()=1000
    open(/dev/dma_heap/system) as unpriv: fd=-1 (errno=2: No such file or directory)
    open(/dev/ashmem) as unpriv: fd=-1 (errno=13: Permission denied)
```

---

## Filp Slab Drain Measurements
Quoting raw serial output from `tier2/evidence/EXP-027/EXP-027_raw_serial.log` (lines 266-293):

```
--- Starting struct file Mass-Freeing Drain Test ---
[BASELINE] slabs=2, objects=32, total_objects=32, objs_per_slab=16, order=0, object_size=232
[*] Forking 40 workers, each opening 800 eventfds (32000 total)...
[*] Waiting for all workers to allocate eventfd files...
[*] All workers have opened 32,000 files. Sampling PEAK slab stats...
[PEAK] slabs=2002, objects=32032, total_objects=32032, objs_per_slab=16, order=0, object_size=232
[*] Signaling workers to mass-close files and exit...
[*] All workers terminated. Waiting for RCU grace periods and SLUB slab reclaim...
    T+1s: slabs=11, objects=115
    T+2s: slabs=11, objects=115
    T+3s: slabs=11, objects=115
    T+4s: slabs=11, objects=115
    T+5s: slabs=11, objects=115
[POST-DRAIN] slabs=11, objects=115, total_objects=176, objs_per_slab=16, order=0, object_size=232

================================================================
EXP-027 FINAL RESULTS & DELTAS
================================================================
Baseline filp slabs:     2 (objects: 32)
Peak filp slabs:         2002 (objects: 32032)
Post-drain filp slabs:   11 (objects: 115)
Allocated slab delta:    +2000 slabs (+32000 objects)
Drained slab delta:      -1991 slabs (-31917 objects)
Drain efficiency:        99.55%
Net slabs remaining:     +9 slabs vs baseline
[+] EXP-027 CONFIRMED: Mass-freeing struct file successfully drains filp slabs back to buddy allocator.
```

---

## Technical Implications for Cross-Cache Reclaim
1. **Slab Drain Feasibility**:
   - `struct file` objects in GKI 6.1.23 are allocated in `order-0` slab pages (16 objects per 4096-byte page, size 232 bytes).
   - Because `filp` slabs are order-0, draining `filp` pages returns standard 4KB physical frames to the buddy allocator's order-0 freelists.
   - SLUB releases these pages immediately after `call_rcu` completes (within 1 second in our measurement).
   - 99.55% of all allocated slab pages were released back to the buddy allocator; residual retention was only 9 slabs (0.45%), showing virtually zero persistent page pinning or fragmentation.

2. **Cross-Cache Target Constraints on GKI 6.1.23**:
   - On kernels where `CONFIG_DMABUF_HEAPS_SYSTEM=y` is compiled, `/dev/dma_heap/system` provides an unprivileged mechanism to allocate arbitrary order-0 pages from the buddy allocator, allowing attackers to re-allocate a page that previously held `struct file`.
   - On this GKI 6.1.23 build, `CONFIG_DMABUF_HEAPS_SYSTEM` is not compiled. Therefore, cross-cache reclaim targeting page-level buddy reallocation cannot use `/dev/dma_heap/system` directly on this specific kernel image without enabling the system heap config, or alternative page allocators (e.g. anonymous memory, pipe buffers, ashmem if accessible, or page-backed SysV IPC) must be evaluated.
