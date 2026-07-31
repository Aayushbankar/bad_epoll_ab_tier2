# EXP-016: kmalloc-192 Spray Target Selection & Allocation Analysis

## Executive Summary
Following EXPERIMENT_PROTOCOL.md Phase: DISCOVERY, we extracted all kernel data structures sized between 129 and 192 bytes from the compiled target `vmlinux` using `pahole`. Each candidate was evaluated based on:
1. Exact struct size fitting into `kmalloc-192`.
2. Allocation pattern (`kmalloc`/`kmem_cache_alloc` vs. `kzalloc`/`__GFP_ZERO`). Zero-initializing allocators overwrite offset 160 with 0, rendering the NULL write ineffective/redundant as a corruption mechanism.
3. Struct layout at offset 160 (the offset of `struct eventpoll.refs` targeted by the UAF NULL write).

---

## 1. Candidate Extraction (pahole)
- **Total structs extracted (129 - 192 bytes)**: 330 candidate structures.

---

## 2. Allocation Filter Results

### Non-Zeroing Candidates (`kmalloc` / `kmem_cache_alloc` only)
These allocation sites do not automatically zero memory upon allocation:
- `gic_kvm_info`
- `vm_area_struct`
- `io_futex_data`
- `nfs_fh`
- `ext4_inode`
- `p9_req_t`
- `inet_peer`
- `genl_family`
- `nfnl_err`
- `uart_8250_em485`
- `rhashtable`

### Dual-Allocation Candidates (Both `alloc` and `zalloc` sites exist)
- `file`
- `rsc`
- `urb`
- `fs_context`
- `cred`
- `worker`
- `rsi`
- `kstat`
- `bpf_local_storage`
- `kretprobe`
- `eventpoll`
- `dentry`
- `log_c`
- `folio`
- `skcipher_walk`
- `ptr_ring`

---

## 3. Offset 160 Field Mapping & Candidate Evaluation

| Candidate Struct | Size (bytes) | Allocation Type | Offset 160 Field | Evaluation & Notes |
| :--- | :--- | :--- | :--- | :--- |
| `nfnl_err` | 168 | Non-zero (`kmalloc`) | `struct netlink_ext_ack extack` (offset 32, size 136) | Offset 160 falls inside `extack.cookie_len` / `_msg_buf`. High attacker control via Netlink error payloads. |
| `cred` | 168 | Dual (`kmem_cache_alloc` / `prepare_creds`) | `struct group_info * group_info` | Pointer field at offset 160. Clearing `group_info` pointer leads to reference leak / crash. |
| `file` | 184 | Dual (`kmem_cache_alloc`) | `union { struct callback_head f_task_work; ... }` (offset 152, size 32) | Offset 160 overlaps `f_task_work.func` / `f_ra`. High utility if sprayed via file descriptor allocation. |
| `rsc` | 176 | Dual (`kmalloc`) | `struct callback_head callback_head` (offset 160, size 16) | Offset 160 contains `callback_head.next` / `func`. RPC GSS security context. |
| `genl_family` | 160 | Non-zero (`kmalloc`) | Struct size is exactly 160 bytes | End of struct (offset 160 is out of bounds / padding). |
| `rhashtable` | 136 | Non-zero (`kmalloc`) | Struct size is 136 bytes | Struct size smaller than 160 bytes. |

---

## 4. Top Selected Candidates for Target Spray

1. **`nfnl_err` (Netlink Error Ack)**
   - **Size**: 168 bytes (`kmalloc-192`)
   - **Allocator**: Plain `kmalloc` (non-zeroing)
   - **Offset 160 Impact**: Overwrites `extack._msg_buf` / `cookie`. Attacker can trigger allocation and control data via Netlink subsystem.

2. **`file` (File Structure)**
   - **Size**: 184 bytes (`kmalloc-192`)
   - **Allocator**: `kmem_cache_alloc` via `alloc_empty_file`
   - **Offset 160 Impact**: Overwrites `f_task_work` / `f_ra` fields. Easily controlled via `open()` / `dup()`.

3. **`cred` (Process Credentials)**
   - **Size**: 168 bytes (`kmalloc-192`)
   - **Allocator**: `kmem_cache_alloc` via `cred_jar`
   - **Offset 160 Impact**: Overwrites `group_info` pointer. Easily allocated via `prepare_creds()` / `copy_creds()`.
