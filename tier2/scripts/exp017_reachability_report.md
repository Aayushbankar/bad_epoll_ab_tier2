# EXP-017 Candidate Reachability Report

## 1. `nfnl_err` (Offset 160: `netlink_ext_ack` / `_msg_buf`)
**Analysis**:
The offset 160 field for `struct nfnl_err` lands inside `struct netlink_ext_ack extack`, specifically the `_msg_buf` or `cookie_len` fields towards the end of the `extack` structure.
In `net/netfilter/nfnetlink.c`, we see:
```c
static void nfnl_err_deliver(struct list_head *err_list, struct sk_buff *skb)
{
	struct nfnl_err *nfnl_err, *next;

	list_for_each_entry_safe(nfnl_err, next, err_list, head) {
		netlink_ack(skb, nfnl_err->nlh, nfnl_err->err,
			    &nfnl_err->extack);
		nfnl_err_del(nfnl_err);
	}
}
```
If we spray `nfnl_err` structures and trigger the UAF NULL write on it, the NULL write will hit `_msg_buf` (a string buffer used for error messages) or a cookie. 
While `netlink_ack` will read this structure to send an acknowledgement message back to userspace, a NULL write (writing an 8-byte 0) into a string buffer simply truncates the error message. It does not overwrite a pointer, function pointer, or size field that could be leveraged for memory corruption, arbitrary read/write, or control flow hijack.

**Verdict for `nfnl_err`**: Low utility. The corrupted field is read, but the effect of a NULL write is benign (message truncation).

---

## 2. `file` (Offset 160: `f_task_work.func`)
**Analysis**:
The `file` struct is 184 bytes. At offset 152 is a union containing `struct callback_head f_task_work`.
`struct callback_head` is 16 bytes:
- offset 0 (struct offset 152): `struct callback_head *next;`
- offset 8 (struct offset 160): `void (*func)(struct callback_head *);`

The NULL write at offset 160 will zero out the `func` function pointer in `f_task_work`.
In `fs/file_table.c`, when a file is closed (last reference dropped) by a normal user thread, `fput()` queues the actual cleanup (`____fput`) using `task_work_add()`:
```c
	if (likely(!in_interrupt() && !(task->flags & PF_KTHREAD))) {
		init_task_work(&file->f_task_work, ____fput);
		if (!task_work_add(task, &file->f_task_work, TWA_RESUME))
			return;
```
`init_task_work` simply sets the `func` pointer to `____fput`.
If our UAF triggers *after* `fput()` calls `init_task_work` but *before* the task work is executed upon return to userspace, we write NULL to `f_task_work.func`.

When the task returns to userspace, it processes the task work list. If it encounters a NULL `func` pointer, it will attempt to call it, resulting in a NULL pointer dereference.
While this guarantees a crash (panic), achieving code execution from a NULL pointer dereference in modern kernels (with SMAP/SMEP/mmap_min_addr) is generally impossible, meaning it acts only as a Denial of Service, not a privilege escalation primitive.

However, there is an alternative union member at offset 152: `struct file_ra_state f_ra;` (size 32 bytes).
A NULL write at offset 160 would overwrite bytes 8-15 of `f_ra`, which correspond to `unsigned int prev_pos` (if 32-bit). This also offers no useful exploitation primitive.

**Verdict for `file`**: Moderate utility. The field is definitely used, but a NULL write to a function pointer only yields a DoS (NULL dereference), and writing to readahead state does nothing useful.

---

## Conclusion
Both remaining candidates have severe limitations for a NULL-write primitive:
1. `file`: Overwriting `f_task_work.func` with NULL leads to an unexploitable NULL pointer dereference crash.
2. `nfnl_err`: Overwriting `extack` buffers with NULL leads to harmless message truncation.

Given the constraints of the primitive (a single 8-byte NULL write), finding a target where a NULL write creates an actionable primitive (like breaking a list, bypassing a check, or extending a size) is critical, and neither of these two perfectly fit that bill for arbitrary execution, though `file` provides a highly reliable crash.
