# Kernel Research Database

**Source commit**: `7e35917775b8b3e3346a87f294e334e258bf15e6`
**Kernel version**: `6.1.23-android14-4-maybe-dirty`
**Toolchain**: `clang-r487747c` (AOSP prebuilt clang-17, symlinked as `clang-r487747`)
**Build target**: `//common:kernel_aarch64`
**Build time**: 1020s
**Artifacts**: `tier2/android/artifacts/{vmlinux,Image,System.map}`

## Compiler Identity

```
Android (10087095, +pgo, +bolt, +lto, -mlgo, based on r487747c)
clang version 17.0.2
Target: x86_64-unknown-linux-gnu (host)
Cross: aarch64 (kernel)
```

## Build Patches

1. `common/tools/bpf/resolve_btfids/Makefile` — propagate `EXTRA_CFLAGS="$(CFLAGS)"` to libsubcmd (host-tool-only compatibility fix)

## Allocator

- `CONFIG_SLUB=y`
- `CONFIG_SLAB_FREELIST_HARDENED=y`
- `CONFIG_SLAB_FREELIST_RANDOM=y`
- `CONFIG_INIT_ON_ALLOC_DEFAULT_ON=y`

## Security Mitigations

| Feature | Config | Notes |
|---|---|---|
| KASLR | `CONFIG_RANDOMIZE_BASE=y` | |
| KASAN | `CONFIG_KASAN=y`, `CONFIG_KASAN_HW_TAGS=y` | ARM MTE-based; disable with `kasan=off` |
| KFENCE | `CONFIG_KFENCE=y` | Sampling-based UAF detection |
| CFI | `CONFIG_CFI_CLANG=y` | Forward-edge control flow integrity |
| Shadow Call Stack | `CONFIG_SHADOW_CALL_STACK=y` | Separate return address stack |
| PAC | `CONFIG_ARM64_PTR_AUTH=y` | Return address signing |
| BTI | `CONFIG_ARM64_BTI=y` | Branch target identification |
| MTE | `CONFIG_ARM64_MTE=y` | Memory tagging extension |
| SELinux | `CONFIG_SECURITY_SELINUX=y` | Mandatory access control |
| Hardened usercopy | `CONFIG_HARDENED_USERCOPY=y` | |
| Stack protector | `CONFIG_STACKPROTECTOR=y` | |
| USERFAULTFD | `CONFIG_USERFAULTFD=y` | Available for heap spray timing |

## CVE-2026-46242 Relevant Symbols

| Symbol | Address | Type |
|---|---|---|
| `ep_remove` | `ffffffc008407db0` | static (text) |
| `eventpoll_release_file` | `ffffffc008407d1c` | global (text) |
| `do_epoll_ctl` | `ffffffc0084080b8` | global (text) |
| `ep_insert` | `ffffffc008408488` | static (text) |
| `ep_free` | `ffffffc008409900` | static (text) |
| `__fput` | `ffffffc0083a09c4` | static (text) |
| `fput` | `ffffffc0083a0824` | global (text) |
| `eventpoll_fops` | `ffffffc009127330` | data |

## Critical Struct Layouts

### struct epitem — 120 bytes (dedicated slab: `eventpoll_epi`)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0 | 24/16 | `rbn` / `rcu` (union) | RB tree node or RCU callback |
| 24 | 16 | `rdllink` | Ready-list link |
| 40 | 8 | `next` | Overflow list pointer |
| 48 | 12 | `ffd` | `{file*, fd}` — **contains dangling file ptr in UAF** |
| 60 | 4 | (hole) | |
| 64 | 8 | `pwqlist` | Poll wait queue entries |
| 72 | 8 | `ep` | Owning eventpoll pointer |
| 80 | 16 | `fllink` | Per-file epitem hlist node |
| 96 | 8 | `ws` | Wakeup source |
| 104 | 16 | `event` | **User-controlled** (`events` + `data`) |

### struct eventpoll — 176 bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 32 | `mtx` (mutex) |
| 80 | 16 | `rdllist` |
| 96 | 8 | `lock` (rwlock) |
| 104 | 16 | `rbr` (RB root) |
| 120 | 8 | `ovflist` |
| 144 | 8 | `file` |

### struct file — 232 bytes

| Offset | Size | Field | Notes |
|---|---|---|---|
| 48 | 4 | `f_lock` | Spinlock protecting `f_ep` |
| 56 | 8 | `f_count` | Reference count |
| 208 | 8 | `f_ep` | **THE VULNERABLE FIELD** |

### struct cred — 192+ bytes

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `usage` (refcount) |
| 4 | 4 | `uid` |
| 8 | 4 | `gid` |
| 20 | 4 | `euid` |
| 24 | 4 | `egid` |

### struct snd_timer_user — 176 bytes (kmalloc-192)

| Offset | Size | Field | Notes |
|---|---|---|---|
| 0x00 | 8 | `timeri` | Pointer to `struct snd_timer_instance` |
| 0x90 | 32 | `ioctl_lock` | `struct mutex` controlling timer ioctls |
| 0xa0 | 8 | `ioctl_lock.wait_list.next` | **STALE STORE TARGET ADDRESS** (`+0xa0`) |
| 0xa8 | 8 | `ioctl_lock.wait_list.prev` | `struct list_head` prev pointer (`+0xa8`) |

## Slab Cache Details

- `eventpoll_epi`: dedicated cache, 120 bytes, `SLAB_HWCACHE_ALIGN|SLAB_PANIC|SLAB_ACCOUNT`
- `eventpoll_pwq`: dedicated cache, `sizeof(struct eppoll_entry)`, `SLAB_PANIC|SLAB_ACCOUNT`
- `ep_head`: dedicated cache, 16 bytes, `SLAB_PANIC|SLAB_ACCOUNT`
- `kmalloc-192`: generic slab cache holding both `struct eventpoll` (192B) and `struct snd_timer_user` (176B).
- Cross-cache exploitation would be needed to replace a freed epitem with a different object type.

