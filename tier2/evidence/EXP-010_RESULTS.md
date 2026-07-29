# EXP-010: File Object UAF - pipe_buffer Content Control Assessment

## Objective
Determine if switching the reclaiming spray from `open()` to `pipe_buffer` allocations provides arbitrary content control over the reclaimed `struct file`, as relied upon in the original x86 exploits.

## Source Code Analysis

### 1. `struct file` Allocation (filp cache)
In `fs/file_table.c:527`, the `filp` cache is created:
```c
filp_cachep = kmem_cache_create("filp", sizeof(struct file), &args,
            SLAB_HWCACHE_ALIGN | SLAB_PANIC |
            SLAB_ACCOUNT | SLAB_TYPESAFE_BY_RCU);
```
- **Size:** `sizeof(struct file)` is `0xe8` (232 bytes), placing it normally in the 256-byte size class.
- **Isolation:** Includes `SLAB_TYPESAFE_BY_RCU`. As explicitly documented in `mm/slab_common.c:500` ("slabs with SLAB_TYPESAFE_BY_RCU can't be merged"), this flag prevents the `filp` cache from being merged with generic `kmalloc` caches. The `filp` cache is therefore a strictly dedicated, isolated slab cache.

### 2. `pipe_buffer` Allocation Mechanics
In `fs/pipe.c:815`, the default `pipe_buffer` array is allocated:
```c
pipe->bufs = kcalloc(pipe_bufs, sizeof(struct pipe_buffer), GFP_KERNEL_ACCOUNT);
```
- **Size:** `sizeof(struct pipe_buffer)` is `0x28` (40 bytes). The default `pipe_bufs` is `PIPE_DEF_BUFFERS` (16), resulting in a default allocation size of 640 bytes (falling into `kmalloc-1k`).
- **Resizing Constraint:** When resizing the pipe via `fcntl(F_SETPIPE_SZ)`, `pipe_set_size()` calculates `nr_slots` (fs/pipe.c:1345). The requested size is rounded to a power-of-2 number of pages via `round_pipe_size()`, forcing `nr_slots` to always be a power of 2 (1, 2, 4, 8, 16, etc.).
- **Impossibility of Size Match:** The resulting allocation sizes (40, 80, 160, 320, 640) mathematically skip the 256-byte size class entirely (160 falls in kmalloc-192, 320 in kmalloc-512).

## Compatibility & Reclaim Mechanics

**Incompatible Caches (Two Fatal Flaws):** 
1. **Cache Isolation:** Because `filp` uses `SLAB_TYPESAFE_BY_RCU`, a freed `struct file` returns to the dedicated `filp` freelist, not to a generic `kmalloc` cache. A `pipe_buffer` allocation draws from `kmalloc-cg-*`, meaning it will NEVER natively overlap with a recently freed `struct file` via simple freelist reuse.
2. **Size Class Mismatch:** Even if `filp` were mergeable, the power-of-2 constraints on `pipe_buffer` slots mean it is mathematically impossible to craft a `pipe_buffer` allocation that lands in the `kmalloc-256` size class required to overlap a `struct file`.

## Conclusion & Requirements for Exploitation

**Status: PASSED (Negative Result Identified)**

The `pipe_buffer` cross-cache reclaim strategy is a complete **dead end** for this kernel configuration due to both cache isolation (`SLAB_TYPESAFE_BY_RCU`) and strict sizing constraints. 

**What WOULD need to hold for exploitation to work:**
Since the `filp` cache is dedicated, the only objects allocated into it are new `struct file` objects. A new `struct file` has its critical fields (like `f_op`) heavily initialized during `__alloc_file` and `vfs_open`, preventing arbitrary attacker-controlled bytes from persisting.

To exploit this UAF, we must pivot away from requiring arbitrary byte-level content control over the `struct file` itself, and instead exploit the UAF as a **Type Confusion / Logic Bug**. By reclaiming the UAF slot with a legitimate `struct file` (e.g., a `timerfd` or `signalfd`), we can manipulate the *new* file via its valid syscalls, and have the stale `epitem` reference it in a confused manner. The next logical step is to attempt same-cache reclaim (proven in EXP-009) using files that offer advantageous `private_data` or `f_op` behaviors.
