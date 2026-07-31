# EXP-017 Candidate Cache Segregation Check

## 1. Allocation of `struct file` (Disqualified)
`struct file` is **disqualified** as a generic spray target due to strict cache isolation.
In `fs/file_table.c`, `struct file` objects are allocated from a dedicated, unmergeable slab cache `filp_cachep`:
```c
void __init files_init(void)
{
    // ...
	filp_cachep = kmem_cache_create("filp", sizeof(struct file), &args,
				SLAB_HWCACHE_ALIGN | SLAB_PANIC |
				SLAB_ACCOUNT | SLAB_TYPESAFE_BY_RCU);
```
The use of `SLAB_TYPESAFE_BY_RCU` forces the SLUB allocator to keep this cache isolated. It will never merge with the generic `kmalloc-192` cache, meaning freeing an `eventpoll` object and spraying `file` objects will never reclaim the freed slot.

## 2. Allocation of `struct eventpoll` (Confirmed Generic)
`struct eventpoll` is confirmed to be allocated via the generic slab allocator (kmalloc caches), meaning it **is** vulnerable to generic heap spraying techniques.
In `fs/eventpoll.c`:
```c
static int ep_alloc(struct eventpoll **pep)
{
	struct eventpoll *ep;

	ep = kzalloc(sizeof(*ep), GFP_KERNEL);
// ...
```
Because `kzalloc` is used directly rather than a dedicated `kmem_cache_alloc` (unlike `epitem` which uses `epi_cache` in the same file), the allocation falls through to the generic `kmalloc-192` cache (or `kmalloc-cg-192` if memcg accounting is applied). 

This confirms the underlying primitive premise: `eventpoll` slots *can* be reclaimed by generic `kmalloc-192` spray objects. We just need to find one.

## Next Steps
Since `file` is disqualified (and `cred` was dropped), and `nfnl_err` has low utility, we must return to the master list of 330 structs sized 129-192 bytes and re-filter specifically for:
1. **Generic Allocation**: Allocated via `kmalloc()` or `kzalloc()` (not a dedicated `kmem_cache`).
2. **High Utility**: Contains function pointers, critical lengths, or list heads at offset 160.
