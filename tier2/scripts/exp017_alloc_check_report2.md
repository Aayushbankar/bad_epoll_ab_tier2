# EXP-017 Candidate Cache Segregation Check (Round 2)

## 1. Fast Filter Script Execution
We created and executed a shell script `filter_generic_alloc_fast.sh` to comprehensively search the kernel tree for the allocation mechanism of our candidates.
The script checks for:
- `kmem_cache_create` containing the structure name, indicating cache segregation.
- `kmalloc` / `kzalloc` enclosing `sizeof(struct [cand])`.

## 2. Re-evaluating Candidate Cache Affinity

### `struct file` (Disqualified)
As verified in Round 1, `struct file` has a dedicated, RCU-isolated slab (`filp_cachep`). It cannot reclaim `kmalloc-192` slots.

### `struct urb` (Generic)
`usb_alloc_urb` in `drivers/usb/core/urb.c` calls:
```c
urb = kmalloc(struct_size(urb, iso_frame_desc, iso_packets), mem_flags);
```
Since it uses `kmalloc` directly (and often `GFP_KERNEL` or `GFP_NOIO`), it allocates from the generic pool.
Size: 160 bytes. (Size is 160 + `iso_packets` * 32, so with `iso_packets = 1` it's 192 bytes, exactly hitting kmalloc-192).

### `struct worker` (Generic - Zeroing)
`alloc_worker` in `kernel/workqueue.c` calls:
```c
worker = kzalloc_node(sizeof(*worker), GFP_KERNEL, node);
```
Since it uses `kzalloc_node`, it goes to the generic pool. However, it zero-initializes the memory, negating our NULL write at offset 160. Disqualified.

### `struct io_futex_data` (Generic)
`io_uring/futex.c`:
```c
return kmalloc(sizeof(struct io_futex_data), GFP_NOWAIT);
```
Uses `kmalloc` (generic pool), non-zeroing.

### `struct nfnl_err` (Generic)
Uses `kmalloc` (generic pool), non-zeroing. But as previously established, utility is low.

### `struct nfs_fh` (Generic)
`fs/nfs/inode.c`:
```c
fh = kmalloc(sizeof(struct nfs_fh), GFP_KERNEL);
```
Uses `kmalloc` (generic pool), non-zeroing.

## 3. Offset 160 Analysis for Generic Non-Zeroing Survivors

We now have these surviving generic-pool, non-zeroing candidates:
1. `io_futex_data`
2. `nfs_fh`
3. `urb` (specifically with `iso_packets=1` making it 192 bytes)
4. `uart_8250_em485`

Let's check their offset 160 fields (from the previous `pahole` analysis):
- `io_futex_data`: PADDING / END_OF_STRUCT
- `nfs_fh`: PADDING / END_OF_STRUCT
- `uart_8250_em485`: PADDING / END_OF_STRUCT
- `urb`: `int interval` at offset 160 (size 4 bytes).

## 4. Conclusion
None of the non-zeroing generic kmalloc-192 candidates have highly exploitable pointers or critical list heads at offset 160.
- `urb` places the 4-byte `interval` integer at offset 160. Writing a NULL here simply sets the USB polling interval to 0. This is not exploitable for control flow or arbitrary memory reads/writes.
- The other candidates fall into padding.

This fundamentally constrains the UAF primitive. The `eventpoll` slot *is* in `kmalloc-192`, but the specific struct size (129-192) and allocator requirements (`kmalloc` vs `kzalloc` vs `kmem_cache`) filter out all highly exploitable candidates (like `file` or `cred` which have their own caches, or `worker` which uses `kzalloc`).

The most viable path forward to demonstrate the vulnerability with a controlled crash is to reconsider the `eventpoll` UAF offset itself, or accept that a 0 write into `nfnl_err`'s string buffer or `urb`'s interval is the maximum achievable corruption directly from this specific 8-byte NULL write, making the bug practically unexploitable for privilege escalation on this specific kernel build, serving only as a memory corruption / Denial of Service vulnerability.
