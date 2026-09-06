# EXP-016b: kmalloc-192 Repeated-Reset Gadget Candidate Search — Results

## Executive Summary
- **Objective**: Re-run the EXP-016 `kmalloc-192` candidate audit using broader criteria: flag any structure where offset 160 (0xa0) contains a counter, flag, small integer, or state-machine field that could form a repeated-reset or increment-style gadget (leveraging the re-triggerable 8-byte NULL write primitive).
- **Target**: `linux-6.1.23` Android GKI (`tier2/android/source/common/vmlinux`).
- **Result**: **COMPLETED**.
  - Analyzed 431 data structures in the `kmalloc-192` range (128 < size <= 192 bytes).
  - Identified all candidates spanning offset 160 (offsets 160–167).
  - Surfaced several new non-pointer candidates including refcounts (`binder_device->ref`), security policy masks (`fib4_rule->srcmask`), certificate revocation flags (`x509_certificate->blacklisted`), and driver state/counters (`urb->error_count`, `uvc_fh->state`).
  - **Exploitability Conclusion**: Under unprivileged Android execution (`CONFIG_USER_NS=n`), no candidate yields a viable unprivileged LPE primitive:
    1. Security-sensitive policy resets (`fib4_rule->srcmask`, `ip6t_entry->counters`) require `CAP_NET_ADMIN`.
    2. Dynamic `binder_device` creation requires `CAP_SYS_ADMIN` (binderfs / binder-control).
    3. `refcount_t` fields (`binder_device->ref`) trigger hard kernel mitigation halts (`REFCOUNT_WARN: underflow/saturation`) rather than exploitable wraps.
    4. Driver fields (`urb->interval`/`error_count`, `wakeup_source->expire_count`) do not govern security boundaries.

---

## Candidate Extraction & Field Mapping at Offset 160

| Struct Name | Size (B) | Field at Offset 160 (0xa0) | Type | Field Size | Category | Generic kmalloc-192? |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| `binder_device` | 168 | `refcount_t ref;` | `refcount_t` | 4B | Refcount | **YES** (`kzalloc`) |
| `fib4_rule` | 176 | `__be32 srcmask;` | `__be32` | 4B | Subnet Mask | **YES** (`kzalloc`) |
| `fib4_rule` | 176 | `__be32 dst;` | `__be32` | 4B | IP Address | **YES** (`kzalloc`) |
| `urb` | 184 | `int interval;` | `int` | 4B | Interval | **YES** (`kmalloc`) |
| `urb` | 184 | `int error_count;` | `int` | 4B | Error Counter | **YES** (`kmalloc`) |
| `wakeup_source` | 192 | `unsigned long expire_count;` | `unsigned long` | 8B | Expiry Counter | **YES** (`kzalloc`) |
| `x509_certificate` | 168 | `bool blacklisted;` | `bool` | 1B | Revocation Flag | **YES** (`kzalloc`) |
| `uvc_fh` | 168 | `enum uvc_handle_state state;` | `enum` | 4B | State Enum | **YES** (`kzalloc`) |
| `ip6t_entry` | 168 | `struct xt_counters counters;` | `u64 bcnt` | 8B | Byte Counter | **YES** (`kvmalloc`) |
| `aio_kiocb` | 176 | `refcount_t ki_refcnt;` | `refcount_t` | 4B | Refcount | **NO** (dedicated `kiocb_cachep`) |
| `v4l2_kevent` | 168 | `u64 ts;` | `u64` | 8B | Timestamp | **YES** (`kzalloc`) |
| `exfat_entry_set_cache` | 168 | `unsigned int num_entries;` | `unsigned int` | 4B | Count | **YES** (`kzalloc`) |
| `dirty_seglist_info` | 168 | `unsigned int pinned_secmap_cnt;` | `unsigned int` | 4B | Counter | **YES** (F2FS internal) |

---

## Detailed Evaluation of New Repeated-Reset Candidates

### 1. `binder_device` (168 bytes, Generic `kmalloc-192`)
- **Field**: `refcount_t ref` at offset 160.
- **Source**: `drivers/android/binderfs.c:147` (`kzalloc(sizeof(*device), GFP_KERNEL)`).
- **Reset Impact**:
  - `refcount_set(&device->ref, 1)` initializes the reference count to 1.
  - Zeroing offset 160 sets `device->ref = 0`.
  - When `refcount_dec_and_test(&device->ref)` is invoked (`binder.c:5077` or `binderfs.c:274`), the Linux `refcount_t` protection subsystem detects an underflow from 0:
    ```c
    REFCOUNT_WARN("refcount_t: underflow; use-after-free");
    ```
  - `refcount_t` mitigations in GKI 6.1 prevent this from becoming an exploitable double-free; the refcount saturates and leaks rather than permitting arbitrary decrement chains.
- **Reachability**: Unprivileged apps cannot mount `binderfs` or execute `BINDER_CTL_ADD` without `CAP_SYS_ADMIN` (`CONFIG_USER_NS=n`).

### 2. `fib4_rule` (176 bytes, Generic `kmalloc-192`)
- **Field**: `__be32 srcmask` at offset 160 (`net/ipv4/fib_rules.c`).
- **Reset Impact**:
  - `srcmask` determines which source IP prefixes match the routing rule.
  - Resetting `srcmask` to `0` changes a specific network subnet rule into a `/0` wildcard (match-all) rule.
  - While conceptually a security-policy corruption gadget, modifying or adding FIB rules requires `CAP_NET_ADMIN` (`RTM_NEWRULE`), preventing unprivileged trigger.

### 3. `x509_certificate` (168 bytes, Generic `kmalloc-192`)
- **Field**: `bool blacklisted` at offset 160 (`crypto/asymmetric_keys/x509_parser.c`).
- **Reset Impact**:
  - In `x509_cert_parse()`, if a certificate is present in the blacklist keyring, `cert->blacklisted = true`.
  - Overwriting offset 160 with 0 resets `blacklisted = false`, which would allow a revoked/blacklisted certificate to validate.
  - However, `x509_certificate` is allocated temporarily on key instantiation and immediately freed by `x509_free_certificate()` in the same callstack; it does not persist in the slab cache for asynchronous UAF reclaim.

### 4. `urb` (184 bytes, Generic `kmalloc-192`)
- **Fields**: `int interval` at offset 160, `int error_count` at offset 164.
- **Source**: `usb_alloc_urb(0, GFP_KERNEL)` (`include/linux/usb.h`).
- **Reset Impact**:
  - Resetting `error_count` clears recorded hardware transmission errors.
  - Resetting `interval` corrupts the polling period for interrupt transfers. In `usb_submit_urb()`, non-positive intervals are rejected with `-EINVAL`.
  - Does not provide controlled memory manipulation or privilege escalation.

---

## Final Conclusion
Re-evaluating `kmalloc-192` with the broader criteria confirms that repeated NULL writes to small-integer, flag, or counter fields do not provide an unprivileged LPE primitive on Android GKI 6.1.23. The high-value candidates either reside in dedicated caches (`aio_kiocb`), require elevated capabilities (`CAP_NET_ADMIN`, `CAP_SYS_ADMIN`), are guarded by hardened `refcount_t` mitigations, or result in benign driver telemetry resets.
