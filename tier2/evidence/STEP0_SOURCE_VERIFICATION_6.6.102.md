# STEP 0: Direct Source Verification of android15-6.6-2025-10_r1

> **Date**: September 13, 2026  
> **Target Tree**: `third_party/android15-6.6-2025-10_r1`  
> **Source URL**: `https://android.googlesource.com/kernel/common`  
> **Tag**: `android15-6.6-2025-10_r1`  
> **Exact Commit**: `3dff304da0a6c0edccc00413c1f03518377ea96d`  
> **Kernel Version**: Linux 6.6.102 (`VERSION = 6`, `PATCHLEVEL = 6`, `SUBLEVEL = 102`)  
> **Verification Standard**: Direct Source Audit per `EXPERIMENT_PROTOCOL.md` (Rule 6: STATIC)

---

## 1. Executive Determination

**VERDICT: CONFIRMED VULNERABLE.**

The exact vulnerable code pattern defining CVE-2026-46242 ("Bad Epoll") is **genuinely and completely present** in this downloaded source tree:
1. The **introducing commit's effective state** (`58c9b016e128`: `refcount_t refcount` in `struct eventpoll`) is **PRESENT** at line 228 of `fs/eventpoll.c`.
2. The **lockless fast-path bypass** in `include/linux/eventpoll.h` (`if (likely(!READ_ONCE(file->f_ep))) return;`) is **PRESENT** at line 45.
3. The **unpinned file access** in `__ep_remove()` (`struct file *file = epi->ffd.file;` without `epi_fget`) is **PRESENT** at line 726 of `fs/eventpoll.c`.
4. The **race window trigger** (`WRITE_ONCE(file->f_ep, NULL);` preceding `hlist_del_rcu(&epi->fllink);`) is **PRESENT** at line 748 of `fs/eventpoll.c`.
5. The **upstream patch** (`a6dc643c6931`: `epi_fget()` pinning inside `__ep_remove()` followed by `fput()`) is **100% ABSENT**. In this tree, `epi_fget()` exists only as an incomplete helper called solely by `ep_item_poll()` (line 917).

---

## 2. Quoted Verbatim Evidence from Downloaded Tree

### A. The Lockless Fast-Path Bypass (`include/linux/eventpoll.h:34-54`)
```c
static inline void eventpoll_release(struct file *file)
{

	/*
	 * Fast check to avoid the get/release of the semaphore. Since
	 * we're doing this outside the semaphore lock, it might return
	 * false negatives, but we don't care. It'll help in 99.99% of cases
	 * to avoid the semaphore lock. False positives simply cannot happen
	 * because the file in on the way to be removed and nobody ( but
	 * eventpoll ) has still a reference to this file.
	 */
	if (likely(!READ_ONCE(file->f_ep)))
		return;

	/*
	 * The file is being closed while it is still linked to an epoll
	 * descriptor. We need to handle this by correctly unlinking it
	 * from its containers.
	 */
	eventpoll_release_file(file);
}
```

### B. Unpinned `struct file` Load & Race Trigger in `__ep_remove()` (`fs/eventpoll.c:724-760`)
```c
static bool __ep_remove(struct eventpoll *ep, struct epitem *epi, bool force)
{
	struct file *file = epi->ffd.file; // LINE 726: Bare load without epi_fget() pin
	struct epitems_head *to_free;
	struct hlist_head *head;

	lockdep_assert_irqs_enabled();

	/*
	 * Removes poll wait queue hooks.
	 */
	ep_unregister_pollwait(ep, epi);

	/* Remove the current item from the list of epoll hooks */
	spin_lock(&file->f_lock);
	if (epi->dying && !force) {
		spin_unlock(&file->f_lock);
		return false;
	}

	to_free = NULL;
	head = file->f_ep;
	if (head->first == &epi->fllink && !epi->fllink.next) {
		/* See eventpoll_release() for details. */
		WRITE_ONCE(file->f_ep, NULL); // LINE 748: Sets f_ep to NULL!
		if (!is_file_epoll(file)) {
			struct epitems_head *v;
			v = container_of(head, struct epitems_head, epitems);
			if (!smp_load_acquire(&v->next))
				to_free = v;
		}
	}
	hlist_del_rcu(&epi->fllink); // LINE 756: Writes into freed eventpoll / unlinks
	spin_unlock(&file->f_lock);  // LINE 757: Dereferences unpinned file after f_ep cleared
	free_ephead(to_free);

	rb_erase_cached(&epi->rbn, &ep->rbr);
...
```

### C. Proof of Commit `58c9b016e128` Effective State (`fs/eventpoll.c:224-229`)
```c
	/*
	 * usage count, used together with epitem->dying to
	 * orchestrate the disposal of this struct
	 */
	refcount_t refcount; // LINE 228: epoll refcounting struct member
```

### D. Incomplete `epi_fget` Fix State (`fs/eventpoll.c:899, 917`)
Grep for `epi_fget` across `fs/eventpoll.c`:
```c
899: static struct file *epi_fget(const struct epitem *epi)
917: 	struct file *file = epi_fget(epi);
```
`epi_fget()` is invoked **exclusively** inside `ep_item_poll()` (line 917). It is **not** called in `__ep_remove()`.

---

## 3. Structural Mechanics in `android15-6.6-2025-10_r1`

1. **Thread A (`close(outer)` or `epoll_ctl(DEL)`)** executes `__ep_remove()`:
   - Sets `WRITE_ONCE(file->f_ep, NULL)` at line 748.
2. **Thread B (`close(inner)`)** enters `__fput()` -> `eventpoll_release(file)`:
   - Reads `file->f_ep == NULL` at line 45 of `include/linux/eventpoll.h`.
   - `if (likely(!READ_ONCE(file->f_ep))) return;` executes the lockless fast-path bailout.
   - `eventpoll_release_file(file)` is **skipped entirely**, bypassing `ep->mtx`.
   - `file` and `inner_ep` proceed to immediate release (`ep_free()` / `file_free_rcu()`).
3. **Thread A resumes**:
   - Executes `hlist_del_rcu(&epi->fllink)` at line 756 on the already-freed `inner_ep` (offset 160 write).
   - Retains a dangling unpinned reference to `file`, which can be stranded in survivor epolls.

The kernel source tree is verified vulnerable. Day 1–2 execution may proceed.
