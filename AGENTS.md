# AGENTS.md — bad_epoll_ab_tier2

> **Migration Note (2026-08-08):** This repo was separated from
> `Aayushbankar/bad-epoll-lab`. Branch `tier2-android-port` is now `main`.
> See [`MIGRATION_LOG.md`](MIGRATION_LOG.md) for full details.

## Repo Overview
- **Goal**: CVE-2026-46242 (epoll UAF) research on Android ARM64 GKI.
- **Current phase**: Tier 2 — exploitability assessment on `linux-6.12.67` (Android 14 GKI).
- **Branch**: `main` *(formerly `tier2-android-port` in the `bad-epoll-lab` monorepo)*
- **Evidence standard**: `tier2/docs/EXPERIMENT_PROTOCOL.md` (10 rules, non-negotiable).

---

## Critical Commands

### Build & Run (from `tier2/`)
```bash
# Compile harness (static, musl)
./aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc -static -O0 -g -o rootfs/harness scripts/<HARNESS>.c -pthread

# Package initramfs
cd rootfs && chmod +x init harness && find . -print0 | cpio --null -ov --format=newc > ../initramfs.cpio

# Launch QEMU (GDB on :1234)
DEBUG=1 ./scripts/run_qemu.sh > /dev/null 2>&1 &
QEMU_PID=$!; sleep 2

# Run GDB script
gdb -batch -q -x scripts/<SCRIPT>.py android/artifacts/vmlinux

# Stop QEMU
kill $QEMU_PID || true; pkill -f qemu-system-aarch64
```

### Key Paths
| What | Path |
|------|------|
| Kernel source | `third_party/linux-6.12.67/` |
| vmlinux (debug) | `third_party/linux-6.12.67/vmlinux` |
| Kernel config | `third_party/linux-6.12.67/.config` |
| Evidence dir | `tier2/evidence/` |
| Scripts dir | `tier2/scripts/` |
| Cross-compiler | `tier2/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc` |
| Rootfs | `tier2/rootfs/` |

---

## Evidence Protocol (Mandatory)

1. **Rule 1**: All raw output → `tier2/evidence/EXP-NNN/` as committed files. Never cite outside paths.
2. **Rule 2**: No `VERIFIED`/`PASSED` until you **read the full raw evidence file** and quote supporting lines.
3. **Rule 4**: Log experiment in `tier2/docs/EXPERIMENT_INDEX.md` as `RUNNING` **before** starting.
3. **Rule 5**: No hardcoded addresses without derivation record (symbol at runtime, or comment with vmlinux build).
4. **Rule 6**: `STATIC` vs `RUNTIME` — never conflate.
5. **Rule 10**: Before claiming done: `git status` clean? `git push`? `git ls-remote origin main` shows hash? If not, say so explicitly.

### Git Commit Format
```
<type>(exp-NNN): <short description>

<body with details>
```
Types: `evidence`, `feat`, `fix`, `docs`, `scripts`

### Per-Experiment Artifacts
- `tier2/evidence/EXP-NNN_RESULTS.md` — structured results
- `tier2/evidence/EXP-NNN_raw_*.log` — raw GDB/output
- `tier2/scripts/exp_NNN_*.py` / `.c` / `.sh` — scripts & harnesses
- Update `tier2/docs/VERIFICATION_LEDGER.md` with next VER-NNN

---

## Verified Ground Truth (Do Not Re-Verify)

| Fact | Source |
|------|--------|
| `struct eventpoll` = 176B → `kmalloc-192` | `ep_alloc()` in `fs/eventpoll.c` |
| Race: outer-close/inner-close → `hlist_del_rcu` writes **NULL at offset 160** of freed `inner_epoll` | VER-026 (EXP-015 HW watchpoint) |
| `msg_msg` (144B user data) reclaims freed `inner_epoll` in `kmalloc-192` | VER-027 (EXP-018) |
| `percpu_counter_dec` uses **outer** eventpoll (valid) — Chain 2 dead | VER-031/032 (EXP-023b) |
| Dual-watch KASLR leak **retracted** — UAF (single-epitem) and kernel-ptr write (multi-epitem) mutually exclusive | VER-033 (EXP-024) |
| No kmalloc-192 struct has exploitable field at offset 160 beyond DoS | EXP-016 audit |

---

## Kernel Source Hotspots

| File | Relevance |
|------|-----------|
| `fs/eventpoll.c:804` | `__ep_remove` |
| `fs/eventpoll.c:788` | `ep_free` |
| `fs/eventpoll.c:870` | `ep_clear_and_put` |
| `include/linux/eventpoll.h` | `eventpoll_release()` lockless fast path |
| `ipc/msgutil.c:57,95` | `alloc_msg`, `load_msg` — msg_msg allocation |
| `include/linux/rbtree_augmented.h` | `rb_erase_cached` |
| `lib/percpu_counter.c` | `percpu_counter_add_batch` |

---

## Common Pitfalls

- **Hardcoded breakpoints**: Use symbol names (`__ep_remove + 0x19c`), not absolute addresses. If hardcoded, comment with vmlinux build.
- **Citing agent-internal paths**: All evidence must be in `tier2/evidence/` committed.
- **Claiming `VERIFIED` without quoting raw log**: Must read and quote the exact lines.
- **Conflating STATIC and RUNTIME**: Label every claim.
- **Using `git rev-parse HEAD` as push proof**: Must show `git ls-remote origin main`.

---

## Environment Notes

- Cross-compiler: `tier2/aarch64-linux-musl-cross/bin/aarch64-linux-musl-gcc` (musl, static)
- Kernel: `linux-6.12.67` (Android 14 GKI, commit `7e35917775b8`)
- QEMU: `virt` machine, `cortex-a57`, 2 CPUs, 2GB RAM
- Kernel cmdline: `kasan=on nokaslr` (KASLR disabled for debugging)
- SysV IPC (`msgsnd`/`msgrcv`) works in this AVD; `CONFIG_SYSVIPC=y`

---

## Experiment Workflow

1. Read `tier2/docs/RUNNER_GUIDE.md` for the experiment list.
2. Create harness + GDB script + launcher in `tier2/scripts/`.
3. Log `RUNNING` in `tier2/docs/EXPERIMENT_INDEX.md`.
4. Run, capture raw log to `tier2/evidence/EXP-NNN_raw_*.log`.
5. Write `EXP-NNN_RESULTS.md` with quoted evidence.
6. Update `VERIFICATION_LEDGER.md` with VER-NNN entries.
7. Commit all 3+ files, push, verify with `git ls-remote`.
