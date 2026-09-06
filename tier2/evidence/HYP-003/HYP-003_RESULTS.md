# HYP-003: Verification of Incomplete Fix in GKI 6.1.23 __ep_remove (no epi_fget pin) — Results

## Executive Summary
- **Hypothesis**: GKI 6.1.23 contains an incomplete fix where `epi->ffd.file` is accessed in `__ep_remove` (and `ep_item_poll`) without `epi_fget()` refcount pinning, leaving the watched file susceptible to dangling pointer exploitation.
- **Result**: **CONFIRMED (VULNERABLE)**.
- **Key Evidence**:
  1. `epi_fget()` does **not exist** anywhere in `tier2/android/source/common/fs/eventpoll.c` (0 matches).
  2. Binary disassembly of `__ep_remove` from `vmlinux` reveals that `epi->ffd.file` is loaded directly at offset +48 (`ldr x22, [x1, #48]`) and used without any atomic refcount increment or `fget` call.
  3. Upstream fix comparison: The complete fix introduced in Linux 6.12+ (commit `52c34fc0f36c`) added `epi_fget()` with `atomic_long_inc_not_zero(&file->f_count)` in `ep_item_poll()`. This pinning logic is completely absent in GKI 6.1.23.

---

## 1. Disassembly of __ep_remove in GKI 6.1.23 vmlinux
Quoting raw disassembly from `tier2/evidence/HYP-003/disasm_ep_remove_raw.txt` (lines 10-40):

```assembly
Dump of assembler code for function __ep_remove:
   0xffffffc00839b3c0 <+0>:	d503233f	paciasp
   0xffffffc00839b3c4 <+4>:	a9bc7bfd	stp	x29, x30, [sp, #-64]!
   0xffffffc00839b3c8 <+8>:	910003fd	mov	x29, sp
   0xffffffc00839b3cc <+12>:	a90153f3	stp	x19, x20, [sp, #16]
   0xffffffc00839b3d0 <+16>:	aa0103f4	mov	x20, x1          // x20 = epi (param 2)
   0xffffffc00839b3d4 <+20>:	a9025bf5	stp	x21, x22, [sp, #32]
   0xffffffc00839b3d8 <+24>:	aa0003f5	mov	x21, x0          // x21 = ep (param 1)
   0xffffffc00839b3dc <+28>:	a90363f7	stp	x23, x24, [sp, #48]
   0xffffffc00839b3e0 <+32>:	b000e997	adrp	x23, 0xffffffc00a0cc000
   0xffffffc00839b3e4 <+36>:	12001c58	and	w24, w2, #0xff
   0xffffffc00839b3e8 <+40>:	f9402033	ldr	x19, [x1, #64]   // x19 = epi->pwqlist
   0xffffffc00839b3ec <+44>:	9134a2f7	add	x23, x23, #0xd28
   0xffffffc00839b3f0 <+48>:	f9401836	ldr	x22, [x1, #48]   // x22 = epi->ffd.file DIRECT LOAD (NO PIN)
   0xffffffc00839b3f4 <+52>:	b40001f3	cbz	x19, 0xffffffc00839b430 <__ep_remove+112>
...
   0xffffffc00839b430 <+112>:	9100c2d7	add	x23, x22, #0x30  // &file->f_lock
   0xffffffc00839b434 <+116>:	aa1703e0	mov	x0, x23
   0xffffffc00839b438 <+120>:	942d160e	bl	0xffffffc008ee0c70 <_raw_spin_lock>
```

### Analysis:
- `x1` holds `epi`.
- Instruction `ldr x22, [x1, #48]` loads `epi->ffd.file` directly into register `x22`.
- `x22` is used immediately to take `spin_lock(&file->f_lock)` (`add x23, x22, #0x30`).
- No reference counting, no `fget()`, and no `epi_fget()` exists between loading `epi->ffd.file` and manipulating `file`.

---

## 2. Source Code Verification (GKI 6.1.23 vs 6.12.67)

### GKI 6.1.23 (`fs/eventpoll.c:921`):
```c
static __poll_t ep_item_poll(const struct epitem *epi, poll_table *pt,
				 int depth)
{
	struct file *file = epi->ffd.file;  /* UNPINNED */
	__poll_t res;

	pt->_key = epi->event.events;
	if (!is_file_epoll(file))
		res = vfs_poll(file, pt);
	else
		res = __ep_eventpoll_poll(file, pt, depth);
	return res & epi->event.events;
}
```

### Upstream 6.12.67 (`fs/eventpoll.c:1037`):
```c
static struct file *epi_fget(const struct epitem *epi)
{
	struct file *file;

	file = epi->ffd.file;
	if (!atomic_long_inc_not_zero(&file->f_count))
		file = NULL;
	return file;
}

static __poll_t ep_item_poll(const struct epitem *epi, poll_table *pt,
				 int depth)
{
	struct file *file = epi_fget(epi);  /* PINNED */
	if (!file)
		return 0;
...
```

---

## 3. Conclusion
GKI 6.1.23 does not have the `epi_fget()` reference-pinning mechanism. The watched `struct file` in `epi->ffd.file` is accessed unpinned, confirming that the prerequisites for the sweetsky123 exploitation path (stranding a dangling `struct file` pointer in an active epoll item) exist on this kernel.
