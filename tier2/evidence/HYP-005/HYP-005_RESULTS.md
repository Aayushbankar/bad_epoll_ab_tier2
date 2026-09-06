# HYP-005: Verification of ep_show_fdinfo Read Primitive in GKI 6.1.23 — Results

## Executive Summary
- **Hypothesis**: `ep_show_fdinfo` is compiled into GKI 6.1.23 and prints `file_inode(epi->ffd.file)->i_ino` for watched epoll items, enabling an arbitrary address read (AAR) primitive from an unprivileged context if `epi->ffd.file` is controlled.
- **Result**: **CONFIRMED**.
- **Key Evidence**:
  1. Source code in `fs/eventpoll.c:941` shows that under `CONFIG_PROC_FS`, `ep_show_fdinfo()` reads `struct inode *inode = file_inode(epi->ffd.file);` and prints `ino:%lx` using `inode->i_ino`.
  2. Runtime verification in QEMU: A test process running as unprivileged UID 1000 successfully opened and read `/proc/self/fdinfo/<epfd>`, receiving `tfd: 4 events: 19 data: 4 pos:0 ino:204a sdev:c`.

---

## 1. Source Code Audit
Quoting `tier2/android/source/common/fs/eventpoll.c` (lines 941-961):

```c
#ifdef CONFIG_PROC_FS
static void ep_show_fdinfo(struct seq_file *m, struct file *f)
{
	struct eventpoll *ep = f->private_data;
	struct rb_node *rbp;

	mutex_lock(&ep->mtx);
	for (rbp = rb_first_cached(&ep->rbr); rbp; rbp = rb_next(rbp)) {
		struct epitem *epi = rb_entry(rbp, struct epitem, rbn);
		struct inode *inode = file_inode(epi->ffd.file);

		seq_printf(m, "tfd: %8d events: %8x data: %16llx "
			   " pos:%lli ino:%lx sdev:%x\n",
			   epi->ffd.fd, epi->event.events,
			   (long long)epi->event.data,
			   (long long)epi->ffd.file->f_pos,
			   inode->i_ino, inode->i_sb->s_dev);
		if (seq_has_overflowed(m))
			break;
	}
	mutex_unlock(&ep->mtx);
}
#endif
```

### Mechanism of Arbitrary Address Read (AAR):
- `file_inode(file)` expands to `file->f_inode`.
- `inode->i_ino` is an 8-byte scalar at `offsetof(struct inode, i_ino)`.
- If an attacker controls the fake `struct file` in a reclaimed page, setting:
  `fake_file->f_inode = target_addr - offsetof(struct inode, i_ino)`
  causes `ep_show_fdinfo()` to dereference `(target_addr - offsetof) + offsetof = target_addr`, reading the 8-byte quadword at `target_addr` and outputting it as a hexadecimal string `ino:%lx`.

---

## 2. QEMU Runtime Verification
Quoting raw serial log from `tier2/evidence/HYP-005/HYP-005_raw_serial.log` (lines 249-265):

```
=== HYP-005: Verify ep_show_fdinfo Read Primitive ===
[*] Watched pipe inode ino = 0x204a (8266)
[*] Dropped to unpriv UID 1000
[+] Read from /proc/self/fdinfo/3 (bytes=115):
pos:	0
flags:	02
mnt_id:	13
ino:	1076
tfd:        4 events:       19 data:                4  pos:0 ino:204a sdev:c

[+] HYP-005 CONFIRMED: ep_show_fdinfo exposes inode ino to unprivileged userspace!
```

---

## 3. Conclusion
The `ep_show_fdinfo` read primitive is present, active, and accessible from unprivileged contexts on GKI 6.1.23.
