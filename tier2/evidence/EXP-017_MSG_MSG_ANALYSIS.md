# EXP-017: msg_msg Spray Target Viability Analysis

## Overview
This report documents the discovery that `struct msg_msg` is a viable — and likely **optimal** — spray target for reclaiming the freed `struct eventpoll` in `kmalloc-192`. This fundamentally changes the exploitability assessment from "LDoS only" to "potentially full LPE."

## 1. msg_msg Allocation Mechanism

### Source: `ipc/msgutil.c:57-66`
```c
static struct msg_msg *alloc_msg(size_t len)
{
    struct msg_msg *msg;
    size_t alen;
    alen = min(len, DATALEN_MSG);
    msg = kmem_buckets_alloc(msg_buckets, sizeof(*msg) + alen, GFP_KERNEL);
    ...
}
```

### Key Facts
- `sizeof(struct msg_msg)` = **48 bytes** (header: m_list, m_type, m_ts, next, security)
- `DATALEN_MSG` = `PAGE_SIZE - sizeof(struct msg_msg)` = `4096 - 48` = **4048 bytes**
- Allocation size = `sizeof(*msg) + alen` = `48 + min(user_len, 4048)`
- With `user_len` in range **81-144 bytes**, total allocation = **129-192 bytes** → **`kmalloc-192`**

### Cache Isolation Check
- `CONFIG_SLAB_BUCKETS` is **not set** → `kmem_buckets_alloc` compiles down to plain `__kmalloc_node_noprof`
- `CONFIG_MEMCG` is **not set** → `GFP_KERNEL_ACCOUNT` flag is ignored; no separate `kmalloc-cg` caches
- `CONFIG_RANDOM_KMALLOC_CACHES` is **not set** → no randomized cache selection
- Both `eventpoll` (`kzalloc(sizeof(*ep), GFP_KERNEL)`) and `msg_msg` (`kmem_buckets_alloc(..., GFP_KERNEL)`) resolve to `KMALLOC_NORMAL` type → **same physical cache**

**Conclusion: `msg_msg` with 81-144 bytes of user data lands in the same `kmalloc-192` slab as `struct eventpoll`. No cache isolation prevents reclaim.**

## 2. User Data Control

### Source: `ipc/msgutil.c:106-107`
```c
alen = min(len, DATALEN_MSG);
if (copy_from_user(msg + 1, src, alen))
    goto out_err;
```

User data is copied directly after the 48-byte `msg_msg` header via `copy_from_user(msg + 1, ...)`. This means:
- **Bytes 0-47**: `msg_msg` header (kernel-controlled: m_list, m_type, m_ts, next, security)
- **Bytes 48-191**: **Fully attacker-controlled** user data

## 3. Multi-Offset UAF Primitive Mapping

The previous analysis fixated on offset 160 (`ep->refs.first`) as the sole UAF write. But `__ep_remove` performs **multiple** operations on the freed `ep` after it's been reclaimed:

| Slab Offset | eventpoll Field | Size | UAF Operation | msg_msg User Byte |
|------------|----------------|------|---------------|-------------------|
| 96 | `ep->lock` (spinlock_t) | 4 | `spin_lock_irq` READ+WRITE | **48-51** (controlled) |
| 104 | `ep->rbr.rb_root.rb_node` | 8 | `rb_erase_cached` **FOLLOWS AS POINTER** | **56-63** (controlled) |
| 112 | `ep->rbr.rb_leftmost` | 8 | `rb_erase_cached` READ+WRITE | **64-71** (controlled) |
| 136 | `ep->user` | 8 | `percpu_counter_dec` **DEREFERENCES AS POINTER** | **88-95** (controlled) |
| 160 | `ep->refs.first` | 8 | `hlist_del_rcu` writes NULL | **112-119** (controlled) |

### Critical Primitives

#### A. `rb_erase_cached` at Offset 104 (Arbitrary Read/Write)
`rb_erase_cached(&epi->rbn, &ep->rbr)` reads `ep->rbr.rb_root.rb_node` (offset 104) and follows it as a pointer to an `rb_node`. The attacker controls this pointer via msg_msg user data bytes 56-63. `rb_erase_cached` will:
1. Read the fake rb_node at the attacker-chosen address
2. Perform tree rotation operations that write values to addresses derived from the fake rb_node's `left`/`right`/`parent` fields

This is a **controlled arbitrary kernel read/write** primitive.

#### B. `percpu_counter_dec` at Offset 136 (Arbitrary Decrement)
`percpu_counter_dec(&ep->user->epoll_watches)` reads `ep->user` (offset 136) as a `struct user_struct *`, then accesses `user->epoll_watches`. The attacker controls the pointer via msg_msg user data bytes 88-95. This gives an **arbitrary decrement** of an 8-byte value at an attacker-chosen kernel address.

## 4. Exploitation Chain (Theoretical)

1. **Trigger the race**: Thread A closes outer epoll, Thread B closes inner epoll. Thread B frees `struct eventpoll` to `kmalloc-192`.
2. **Spray `msg_msg`**: Send SysV IPC messages of 144 bytes. The spray reclaims the freed eventpoll slot. User data fills offsets 48-191 with attacker-controlled values.
3. **Thread A resumes**: Thread A's `__ep_remove` operates on the reclaimed slot:
   - `spin_lock_irq(&ep->lock)` at offset 96 → must be set to 0 (unlocked) to avoid deadlock
   - `rb_erase_cached(&epi->rbn, &ep->rbr)` at offset 104 → follows fake rb_node pointer → **arbitrary write**
   - `percpu_counter_dec(&ep->user->epoll_watches)` at offset 136 → follows fake user pointer → **arbitrary decrement**
4. **Weaponize**: Use the arbitrary write/decrement to overwrite `struct cred` fields (e.g., decrement `uid` from non-zero to 0) or corrupt `modprobe_path`.

## 5. Why This Was Missed

The previous candidate search methodology had three compounding blind spots:
1. **Fixed-size-only search**: `pahole` only finds structs with fixed sizes. `msg_msg` is a 48-byte struct with variable-length trailing data — it never appears in the 129-192 byte pahole output.
2. **Single-offset fixation**: Only offset 160 was analyzed. The `rb_erase_cached` (offset 104) and `percpu_counter_dec` (offset 136) primitives were completely ignored.
3. **"NULL write = unexploitable" assumption**: A NULL write at offset 160 IS low-value. But the `rb_erase_cached` primitive at offset 104 is NOT a NULL write — it's a complex tree operation that reads and writes to attacker-controlled addresses.

## 6. Next Steps
1. Build a GDB script to verify `msg_msg` actually reclaims the freed eventpoll slot in practice
2. Map `struct user_struct` to determine the exact offset of `epoll_watches` for the arbitrary decrement
3. Determine whether `rb_erase_cached` can be reliably weaponized given the `epi->rbn` state at the time of the call
4. Check whether `spin_lock_irq` on a user-controlled value causes issues (needs to pass as "unlocked")
