# EXP-007: Cache Isolation and Same-Cache Reclaim

## Objective
Identify whether `eventpoll_epi` (120 bytes) is isolated from `kmalloc-128`, and if so, verify same-cache reclaim using another `epitem` allocation via `epoll_ctl(EPOLL_CTL_ADD)`.

## Results
1. **Cache Isolation Confirmed**: `eventpoll.c` creates `epi_cache` with `SLAB_ACCOUNT`. `mm/slab_common.c` requires `SLAB_MERGE_SAME` flags to match for caches to merge. `kmalloc-128` lacks `SLAB_ACCOUNT` unless `CONFIG_MEMCG` is enabled (and if enabled, `kmalloc-128` becomes unmergeable anyway via `SLAB_NO_MERGE`). Thus, `eventpoll_epi` is isolated. Cross-cache grooming is impossible.
2. **Reclaim Confirmed**: A GDB script (`gdb_epoll_spray.py`) broke on `call_rcu` during `__ep_remove` (capturing the freed `epitem` pointer), then monitored `kmem_cache_alloc` returns during `do_epoll_ctl`. The second spray allocation exactly matched the freed address (`0xffffff80043cb400`).

## Evidence
- `tier2/evidence/EXP-007_raw_gdb.log`

## Next Steps
Proceed to Stage 6: investigate how the stale `list_del_init` corrupts the *newly allocated* `epitem` at offset 24 and 32, and determine if those corrupted fields (like `rdllink` or `ffd`) can be weaponized.
