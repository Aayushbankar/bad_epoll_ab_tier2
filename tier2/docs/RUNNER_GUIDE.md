# RUNNER GUIDE: Epoll UAF Exploit Chain Experiments

> **FOR**: A model/agent executing experiments in this repo.
> **GOAL**: Execute every experiment, hypothesis, and verification below. Return ALL results.
> **DATE**: 2026-08-01

---

## BEFORE YOU START: MANDATORY RULES

Read `tier2/docs/EXPERIMENT_PROTOCOL.md` in full. The 10 rules are non-negotiable. The most critical:

1. **Rule 1**: All evidence goes into `tier2/evidence/` as committed files. NEVER cite paths outside the repo.
2. **Rule 2**: No `VERIFIED`/`PASSED` status until you have READ the raw evidence file in full and QUOTED the specific lines that support the claim.
3. **Rule 4**: Log the experiment in `tier2/docs/EXPERIMENT_INDEX.md` as `RUNNING` BEFORE you start. Update status AFTER.
4. **Rule 10**: Before reporting ANYTHING as done: `git status` (clean?), `git push`, then `git ls-remote origin main` to confirm the hash is on the remote. Show the `ls-remote` output. Never use `git rev-parse HEAD` as proof of push.

### Git Commit Format
```
<type>(exp-NNN): <short description>

<body with details>
```
Types: `evidence`, `feat`, `fix`, `docs`, `scripts`

### Evidence File Format
Every experiment produces:
- `tier2/evidence/EXP-NNN_RESULTS.md` — structured results doc (see EXP-015_RESULTS_HW_TRACE.md for format template)
- `tier2/evidence/EXP-NNN_raw_*.log` — raw command/GDB output
- `tier2/scripts/exp_NNN_*.py` or `.c` or `.sh` — scripts and harnesses

### Verification Ledger
Update `tier2/docs/VERIFICATION_LEDGER.md` for every new verified/disproved claim. Use the next VER-NNN ID in sequence.

---

## ENVIRONMENT REFERENCE

### How to Compile a C Harness
```bash
# Updated 2026-08-08: use repo-relative path after repo separation (was absolute bad-epoll-lab path)
cd "$(git rev-parse --show-toplevel)/tier2"
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/<YOUR_HARNESS>.c -pthread
```

### How to Package the rootfs
```bash
cd tier2/rootfs
chmod +x init harness
find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio 2>/dev/null
```

### How to Launch QEMU
```bash
cd tier2
DEBUG=1 ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!
sleep 2
```
This starts QEMU with the kernel at `android/artifacts/vmlinux` (or `third_party/linux-6.12.67/vmlinux`) and opens GDB port `:1234`.

### How to Run a GDB Script
```bash
gdb -batch -q -x scripts/<YOUR_SCRIPT>.py android/artifacts/vmlinux
```

### How to Stop QEMU
```bash
kill $QEMU_PID || true
pkill -f qemu-system-aarch64 || true
```

### Key Paths
| What | Path |
|------|------|
| Kernel source | `third_party/linux-6.12.67/` |
| Compiled vmlinux | `third_party/linux-6.12.67/vmlinux` or `tier2/android/artifacts/vmlinux` |
| Kernel config | `third_party/linux-6.12.67/.config` |
| Evidence directory | `tier2/evidence/` |
| Scripts directory | `tier2/scripts/` |
| Docs directory | `tier2/docs/` |
| Cross-compiler | `tier2/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc` |
| Rootfs | `tier2/rootfs/` |

### Key Kernel Source Files
| File | Contains |
|------|----------|
| `fs/eventpoll.c` | `__ep_remove` (line 804), `ep_free` (line 788), `ep_clear_and_put` (line 870) |
| `include/linux/eventpoll.h` | `eventpoll_release()` lockless fast path |
| `ipc/msgutil.c` | `alloc_msg` (line 57), `load_msg` (line 95) — msg_msg allocation |
| `include/linux/rbtree_augmented.h` | `rb_erase_cached`, `__rb_erase_augmented` |
| `lib/percpu_counter.c` | `percpu_counter_add_batch` |

---

## KNOWN FACTS (DO NOT RE-VERIFY — these are settled)

1. `struct eventpoll` is **192 bytes** (`sizeof` = 176, rounds up to `kmalloc-192`). Allocated via `kzalloc(sizeof(*ep), GFP_KERNEL)` in `ep_alloc()`. This is **generic kmalloc**, NOT a dedicated cache.
2. The race: Thread A (`__ep_remove`) does `WRITE_ONCE(file->f_ep, NULL)`. If preempted, Thread B (`__fput → eventpoll_release`) sees `f_ep == NULL`, skips cleanup, frees the eventpoll. Thread A resumes and writes to freed memory. **Proven by EXP-015 (VER-026).**
3. `struct msg_msg` header = **48 bytes**. With user data of 81-144 bytes, total = 129-192 → `kmalloc-192`. Allocated via `kmem_buckets_alloc` which falls back to plain `kmalloc` because `CONFIG_SLAB_BUCKETS=n`.
4. `CONFIG_MEMCG=n` → no separate cgroup caches. `CONFIG_RANDOM_KMALLOC_CACHES=n`. `CONFIG_SLAB_FREELIST_HARDENED=n`. `CONFIG_INIT_ON_FREE=n`. `CONFIG_USERFAULTFD=n`. `CONFIG_SYSVIPC=y`. `CONFIG_RANDOMIZE_BASE=y` (KASLR).
5. Thread A's UAF operations on the freed slot (in execution order):
   - Line 836: `hlist_del_rcu(&epi->fllink)` → writes NULL to offset 160
   - Line 840: `rb_erase_cached(&epi->rbn, &ep->rbr)` → reads/writes offsets 104-119
   - Line 842: `spin_lock_irq(&ep->lock)` → reads/writes offset 96
   - Line 845: `spin_unlock_irq(&ep->lock)` → writes offset 96
   - Line 857: `percpu_counter_dec(&ep->user->epoll_watches)` → dereferences pointer at offset 136
6. In msg_msg: user data starts at byte 48. So offset 96 = user byte 48, offset 104 = user byte 56, offset 136 = user byte 88, offset 160 = user byte 112. **ALL attacker-controlled.**

---

## EXPERIMENTS TO EXECUTE

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-018: msg_msg Spray Reclaim Verification
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: Prove that a `msg_msg` allocation of 144 bytes of user data actually lands in `kmalloc-192` AND can reclaim a freed `struct eventpoll` slot.

**What to build**:
1. A C harness (`test_exp018.c`) that:
   - Creates an inner epoll, adds a pipe fd to it
   - Creates an outer epoll monitoring the inner epoll fd
   - Prints the inner epoll's `struct eventpoll` address (read via `/proc/self/fdinfo/<fd>` or pass to GDB)
   - Thread A: `close(outer_epoll_fd)` — will be suspended by GDB
   - Thread B: `close(inner_epoll_fd)` — frees inner eventpoll
   - Spray thread: calls `msgsnd()` in a tight loop, sending messages with 144 bytes of data filled with a recognizable pattern (e.g., `0x41414141...`)
   - After spray, prints completion

2. A GDB script (`exp018_gdb.py`) that:
   - Connects to QEMU on `:1234`
   - Sets breakpoint at `__ep_remove + 0x19c` (after `WRITE_ONCE`)
   - When hit: patches Thread A's PC with infinite loop (`set *(int*)($pc) = 0x14000000`)
   - Sets breakpoint at `ep_free` to capture the freed `struct eventpoll` address
   - After `ep_free` completes: reads the freed slot's memory (`x/24gx <addr>`)
   - Continues execution to let the spray thread run
   - After ~1 second: reads the freed slot's memory again (`x/24gx <addr>`)
   - **Verification**: the memory should now contain the `0x41414141...` pattern from the msg_msg spray, proving reclaim

3. A launcher script (`run_exp018.sh`) following the `run_exp015.sh` template.

**Evidence required**:
- `tier2/evidence/EXP-018_raw_gdb.log` — full GDB output
- `tier2/evidence/EXP-018_RESULTS.md` — structured results with before/after memory dumps
- Quote the exact hex dump lines showing the pattern change

**Success criteria**: The memory at the freed eventpoll address changes from kernel data to the `0x41` pattern after `msgsnd()` spray.

**Failure criteria**: Memory does not change, or msg_msg lands in a different cache.

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-019: Controlled Crash PoC (Chain 0)
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: Trigger a controlled kernel panic by spraying msg_msg with an invalid pointer at offset 136, causing `percpu_counter_dec` to dereference garbage.

**What to build**:
1. A C harness (`test_exp019.c`) identical to EXP-018 but the spray data is crafted:
   - Bytes 0-47 (msg_msg header positions when offset from start of user data): set bytes 48-51 of user data (slab offset 96, spinlock) to `0x00000000` (unlocked)
   - Bytes 56-63 of user data (slab offset 104, rb_root.rb_node): set to `0x0000000000000000` (NULL — safe for rb_erase_cached with single-epitem tree, since parent=NULL causes `root->rb_node = NULL` which just writes NULL here)
   - Bytes 64-71 of user data (slab offset 112, rb_leftmost): set to `0x0000000000000000`
   - Bytes 88-95 of user data (slab offset 136, ep->user): set to `0xDEAD000000000000` (invalid kernel pointer)
   - All other bytes: `0x00`

   **IMPORTANT OFFSET CALCULATION**: msg_msg user data starts at slab offset 48. So to write to slab offset X, write to user data byte `X - 48`. Examples:
   - Slab offset 96 (spinlock) = user data byte 48
   - Slab offset 104 (rb_root) = user data byte 56
   - Slab offset 136 (ep->user) = user data byte 88

2. A GDB script (`exp019_gdb.py`) that:
   - Same race setup as EXP-018
   - After `ep_free` and spray, restores Thread A's instruction and continues
   - Expects a kernel panic/Oops at `percpu_counter_dec` or `percpu_counter_add_batch`
   - Captures the full backtrace and register dump

3. A launcher script (`run_exp019.sh`)

**Evidence required**:
- `tier2/evidence/EXP-019_raw_gdb.log` — full GDB output including the panic
- `tier2/evidence/EXP-019_RESULTS.md` — structured results
- MUST QUOTE the backtrace showing `percpu_counter_add_batch` called from `__ep_remove`

**Success criteria**: Kernel Oops/panic with backtrace containing `percpu_counter_add_batch` ← `__ep_remove`.

**What this proves**: The UAF is triggerable, msg_msg reclaims the slot, and Thread A dereferences attacker-controlled data as a pointer. This is a valid CVE PoC.

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-020: rb_erase_cached Behavior Analysis (Single Epitem)
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: Trace what `rb_erase_cached` actually does when the freed eventpoll's `rbr` (offsets 104-119) contains attacker-controlled data. Determine if this is exploitable beyond a simple NULL write.

**What to build**:
1. Use the same harness as EXP-018 (single pipe fd added to inner epoll = single epitem in tree).
2. GDB script (`exp020_gdb.py`) that:
   - Same race setup
   - After spray: sets a hardware watchpoint on offset 104 (`ep->rbr.rb_root.rb_node`)
   - Also sets a breakpoint at `rb_erase_cached` or at `__ep_remove + <offset of rb_erase_cached call>`
   - Resumes Thread A
   - When breakpoint/watchpoint hits: dumps registers, prints what values are read from offsets 104 and 112
   - Steps through the rb_erase_cached execution instruction by instruction (stepi for ~20 instructions)
   - Records all memory reads and writes

**Evidence required**:
- `tier2/evidence/EXP-020_raw_gdb.log` — full GDB stepi trace
- `tier2/evidence/EXP-020_RESULTS.md` — analysis of what rb_erase_cached actually did

**Key questions to answer**:
- Does `rb_erase_cached` read the attacker-controlled value at offset 104?
- Does it follow it as a pointer (i.e., dereference it)?
- Or does it just compare and write NULL?
- What is `epi->rbn.__rb_parent_color` at this point? (Read it from GDB: `p/x *(long*)(&epi->rbn)`)
- What is `epi->rbn.rb_left`? `epi->rbn.rb_right`?

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-021: rb_erase_cached Behavior with TWO Epitems
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: Same as EXP-020 but with TWO fds added to the inner epoll (creating a 2-node rb tree). This changes the rb_erase behavior: the node may have children, parent pointers point to other nodes, and rebalancing may occur.

**What to build**:
1. Harness adds TWO pipe fds to the inner epoll via `epoll_ctl(EPOLL_CTL_ADD)`.
2. Same GDB setup as EXP-020 but trace what addresses rb_erase reads/writes.

**Key questions**:
- Does rb_erase_cached write any kernel heap addresses back into offsets 104 or 112 of the reclaimed slot?
- If so, can these addresses be read back via `msgrcv()` (info leak)?

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-022: Info Leak via msg_msg Read-Back (Chain 1)
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: After Thread A's UAF operations corrupt the msg_msg data, read the msg_msg back via `msgrcv()` and check if kernel pointers leaked into the user data.

**What to build**:
1. Harness (`test_exp022.c`):
   - Same race trigger as before
   - Spray uses `msgsnd()` to a known message queue with `mtype = 0x1337`
   - After Thread A finishes (use a futex/sleep to coordinate), call `msgrcv()` with `mtype = 0x1337`
   - Print all 144 bytes of the received message data in hex
   - Compare against the original sent data — any differences are kernel writes (leaks)

2. GDB script to force the timing (same approach as EXP-019) but let Thread A run to completion WITHOUT crashing. For this:
   - Set offset 136 (ep->user) to the address of the REAL `init_user` global variable (look up via `p &init_user` in GDB), so `percpu_counter_dec` succeeds on a real user_struct
   - OR: determine `ep->user`'s original value before the free (it's the current process's user_struct) and put that value back

3. After Thread A completes, the harness calls `msgrcv()` and prints the data.

**Evidence required**:
- Hex dump of sent data vs received data
- Identify which bytes changed and what they changed to
- If bytes 56-71 (slab offsets 104-119) now contain non-zero kernel pointers, that's an info leak

**Success criteria**: At least one byte of the received msg_msg data differs from the sent data AND the new value looks like a kernel pointer (starts with `0xffff` on aarch64).

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-023: percpu_counter_dec Arbitrary Decrement (Chain 2)
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: Demonstrate that `percpu_counter_dec` can be made to decrement a value at an attacker-chosen kernel address.

**Prerequisite**: EXP-022 succeeds (info leak for KASLR bypass) OR use GDB to directly read kernel addresses.

**What to build**:
1. Map the `percpu_counter_dec` pointer chase. Given `ep->user` = `fake_ptr`:
   - `fbc` = `fake_ptr + 8` (offset of `epoll_watches` in `user_struct`)
   - `fbc->lock` = `*(fake_ptr + 8)` (raw_spinlock_t, must be 0 = unlocked)
   - `fbc->count` = `*(fake_ptr + 16)` (s64, this gets incremented by `count + amount`)
   - `fbc->counters` = `*(fake_ptr + 40)` (per-CPU s32 pointer, DEREFERENCE TARGET)
   - The actual decrement: `*per_cpu_ptr(fbc->counters, cpu)` is decremented by 1

2. Place a fake `user_struct` in kernel memory via a SECOND msg_msg. The second msg_msg's user data (at a predicted heap address) contains:
   - Offset 8 (fbc->lock): `0x00000000` (unlocked)
   - Offset 16 (fbc->count): `0x0000000000000000`
   - Offset 40 (fbc->counters): pointer to the TARGET address (e.g., `modprobe_path`)

3. Set `ep->user` (slab offset 136, msg_msg byte 88) to `fake_user_struct_addr - 8` (so that `user + 8` = fake percpu_counter start)

**This is complex. Start with a GDB-assisted version**: Use GDB to read the actual `modprobe_path` address and the actual heap address of the second msg_msg, and manually construct the pointers. Document every address and offset.

**Evidence required**:
- GDB trace showing `percpu_counter_dec` following the fake pointer chain
- Before/after memory dump of the target address showing the decrement
- If targeting `modprobe_path`: show the string changed from `/sbin/modprobe` to something else

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-024: Automated Race Without GDB
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: Trigger the epoll UAF race without GDB, using only userspace timing techniques. This is required for a standalone PoC binary.

**What to build**:
1. A standalone C program (`test_exp024_race.c`) that:
   - Creates the epoll topology (inner + outer)
   - Uses `sched_setaffinity()` to pin Thread A to CPU 0 and Thread B to CPU 1
   - Uses `SCHED_FIFO` scheduling if available (`sched_setscheduler`)
   - Uses a shared `volatile int` flag for synchronization:
     - Thread A: `close(outer_fd)` → sets flag=1 after WRITE_ONCE executes (we can't control this from userspace, but the race window exists naturally)
     - Thread B: spins on flag, then immediately `close(inner_fd)`
   - Actually, the simplest approach: just have both threads call `close()` simultaneously and rely on statistical probability
   - Run in a tight loop for 100,000 iterations
   - After each iteration, check if `/proc/slabinfo` or kernel logs show corruption, or check for specific dmesg output via `klogctl()`

2. Detection: After each race attempt, try to trigger an operation that would crash if the UAF occurred. For example:
   - After close, immediately spray msg_msg
   - Then check if dmesg contains any Oops

**ALTERNATIVE approach — use `FUTEX_WAIT_BITSET` for timing**:
```c
// Thread A: close(outer_fd) then futex_wake
// Thread B: futex_wait, then immediately close(inner_fd)
// The close() syscalls should interleave
```

**Evidence required**:
- Whether the race was ever hit without GDB
- If hit: how many iterations it took
- dmesg output showing the crash/corruption

---

## ADDITIONAL VERIFICATIONS TO DO

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### VER-027: Confirm eventpoll Uses KMALLOC_NORMAL (Not KMALLOC_RECLAIM)
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Task**: `eventpoll` uses `kzalloc(sizeof(*ep), GFP_KERNEL)`. `GFP_KERNEL` does NOT set `__GFP_DMA`, `__GFP_RECLAIMABLE`, or `__GFP_ACCOUNT`. Therefore `kmalloc_type()` returns `KMALLOC_NORMAL`. Verify this at runtime.

**Method**: In GDB, after the eventpoll is allocated, check which slab cache it came from:
```
(gdb) p (struct kmem_cache *)((struct slab *)virt_to_slab(ep))->slab_cache
```
Or simpler: check `/proc/slabinfo` for `kmalloc-192` usage before and after creating an epoll fd.

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### VER-028: Confirm msg_msg Uses Same kmalloc-192 Cache
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Task**: Verify at runtime that a `msgsnd()` with 144 bytes of data allocates from the same `kmalloc-192` cache as eventpoll.

**Method**: Check `/proc/slabinfo` for `kmalloc-192` active_objs before and after `msgsnd()`. The count should increase.

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### VER-029: Verify spin_lock_irq Behavior on User-Controlled Value
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Task**: Confirm that if the spinlock at offset 96 is set to `0x00000000` (unlocked), `spin_lock_irq` succeeds and `spin_unlock_irq` completes without issues. Also confirm that if set to `0x00000001` (locked), `spin_lock_irq` spins forever (soft lockup).

**Method**: GDB trace through the `spin_lock_irq` call at line 842 with different values at offset 96.

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### VER-030: Verify msgsnd/msgrcv Are Accessible from Android App Context
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Task**: Even though `CONFIG_SYSVIPC=y`, Android's seccomp profile may block `msgsnd`/`msgrcv` syscalls for app processes. Verify by running a test binary that calls `msgget()`, `msgsnd()`, and `msgrcv()` on the AVD.

**Method**: Write a minimal C program that does:
```c
int msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
if (msqid < 0) { perror("msgget"); return 1; }
// ... msgsnd, msgrcv ...
```
Run it on the AVD. If `msgget` returns `ENOSYS` or `-EPERM`, SysV IPC is blocked.

**If blocked**: Fall back to `add_key`/`keyctl` (`CONFIG_KEYS=y`) or `setxattr` (`CONFIG_TMPFS_XATTR=y`) as spray alternatives. Document which one works.

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### VER-031: Map init_user Address for Safe percpu_counter_dec
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Task**: Find the address of the global `init_user` (`struct user_struct`) in the kernel. This is needed for EXP-022 (info leak) — we need a valid `user_struct` pointer to put at offset 136 so Thread A doesn't crash at `percpu_counter_dec`.

**Method**:
```
(gdb) p &init_user
(gdb) p/x &init_user.epoll_watches
(gdb) p/x init_user.epoll_watches.counters
```
Record these addresses.

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### VER-032: Map modprobe_path Address for Chain 2
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Task**: Find the address of `modprobe_path` in the running kernel.

**Method**:
```
(gdb) p &modprobe_path
(gdb) x/s &modprobe_path
```
Should show `/sbin/modprobe`. Record the address.

---

## ADDITIONAL EXPERIMENTS WORTH TRYING

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-025: setxattr as Alternative Spray
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: If SysV IPC is blocked (VER-030), verify `setxattr` can spray `kmalloc-192`.

**Method**: 
```c
// Mount tmpfs, create file, set xattr with 129-192 bytes of controlled data
mount("tmpfs", "/tmp/spray", "tmpfs", 0, NULL);
int fd = open("/tmp/spray/target", O_CREAT|O_RDWR, 0666);
char data[144]; // fills kmalloc-192
memset(data, 0x41, 144);
fsetxattr(fd, "user.spray", data, 144, 0);
```
Check if the xattr value lands in `kmalloc-192` via `/proc/slabinfo`.

Note: `setxattr` allocates temporarily (for the value copy) then stores it in the inode. The temporary buffer is freed. For heap spray purposes, you need MANY xattrs on MANY files to keep the allocations alive, OR use `fsetxattr` in a loop with different attribute names.

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-026: add_key as Alternative Spray
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: Verify `add_key` can spray `kmalloc-192` with controlled data.

**Method**:
```c
// key payload is kmalloc'd
key_serial_t key = add_key("user", "spray_key", payload, payload_len, KEY_SPEC_PROCESS_KEYRING);
```
Check payload_len values that hit `kmalloc-192` (129-192 bytes). Verify with `/proc/slabinfo`.

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-027: Full End-to-End LPE PoC (Chain 3)
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: Combine info leak + arbitrary write into a single binary that achieves root.

**Prerequisites**: EXP-022 (info leak) AND EXP-023 (arbitrary write) both succeed.

**Build**: A single C program that:
1. Triggers the race once → sprays msg_msg with `init_user` at offset 136 → reads back msg_msg → leaks kernel heap address
2. Uses leaked address to calculate KASLR base and `modprobe_path` address
3. Triggers the race a second time → sprays msg_msg with crafted fake `user_struct` at offset 136 → `percpu_counter_dec` overwrites `modprobe_path`
4. Creates `/tmp/evil` with `chmod u+s /bin/sh` content
5. Triggers modprobe by executing a file with unknown binary format
6. `/bin/sh` is now suid root → `execve("/bin/sh", ...)`

---

### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
### EXP-028: Kernel Stack Info Leak via Oops Backtrace
### ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

**Objective**: Even without a controlled info leak, a kernel Oops prints register values and a backtrace to dmesg. If we trigger Chain 0 (controlled crash), the Oops output may contain enough kernel addresses to defeat KASLR.

**Method**:
1. Run EXP-019 (controlled crash)
2. After the crash, read dmesg from the serial console or QEMU output
3. Extract all `0xffff...` addresses from the Oops
4. Determine which are kernel text (`.text` segment) and which are heap
5. Calculate KASLR base from a known `.text` symbol offset

---

## EXPERIMENT EXECUTION ORDER

Execute in this order. Each experiment may depend on results from previous ones.

```
1. EXP-018  (msg_msg reclaim verification)           — Foundation
2. VER-027  (eventpoll cache verification)            — Foundation
3. VER-028  (msg_msg cache verification)              — Foundation
4. VER-030  (msgsnd/msgrcv availability on AVD)       — Foundation
5. EXP-019  (controlled crash — Chain 0)              — First PoC
6. EXP-020  (rb_erase single epitem trace)            — Primitive analysis
7. EXP-021  (rb_erase two epitems trace)              — Primitive analysis
8. VER-029  (spinlock behavior verification)          — Safety check
9. VER-031  (init_user address mapping)               — Prep for Chain 1
10. VER-032 (modprobe_path address mapping)           — Prep for Chain 2
11. EXP-022 (info leak via msg_msg read-back)         — Chain 1
12. EXP-028 (Oops backtrace info leak)                — Alternative Chain 1
13. EXP-023 (arbitrary decrement)                     — Chain 2
14. EXP-024 (automated race without GDB)              — Standalone PoC
15. EXP-025 (setxattr alternative spray)              — Backup
16. EXP-026 (add_key alternative spray)               — Backup
17. EXP-027 (full LPE)                                — Final goal
```

## REPORTING FORMAT

For EVERY experiment, create the results doc in this exact format:

```markdown
# EXP-NNN: <Title>

## Objective
<One paragraph>

## Methodology
<Numbered steps>

## Results
<Raw output excerpts with line numbers>

## Analysis
<What the results mean>

## Conclusion
<PASSED / FAILED / INCONCLUSIVE with one-line justification>
```

## FINAL CHECKLIST AFTER ALL EXPERIMENTS

- [ ] All experiments logged in `tier2/docs/EXPERIMENT_INDEX.md`
- [ ] All new verified claims added to `tier2/docs/VERIFICATION_LEDGER.md`
- [ ] All evidence files committed to `tier2/evidence/`
- [ ] All scripts committed to `tier2/scripts/`
- [ ] `git status` is clean
- [ ] `git push origin main` succeeded
- [ ] `git ls-remote origin main` output shown with commit hash
