# EXP-017 Candidate Analysis: Final Conclusion

## 1. Methodology Correction (Semantic Analysis)
Following the identification of the methodological gap in text-matching `sizeof(struct NAME)`, we abandoned `grep` in favor of semantic analysis. We wrote a Python script (`semantic_alloc_finder.py`) that:
1. Re-verified dedicated caches (`kmem_cache_create`).
2. Iterated over all `.c` files in the kernel source tree (`third_party/linux-6.12.67`).
3. Parsed variable declarations of the form `struct <candidate> *var;`.
4. Extracted assignments to those variables involving `kmalloc`, `kzalloc`, etc.
5. Successfully segregated all 330 candidates into "Generic Non-Zeroing", "Generic Zeroing", and "Dedicated Cache".

This approach correctly identified candidates using the `var = kmalloc(sizeof(*var), ...)` idiom, completely resolving the blind spot.

## 2. Updated Surviving Candidates
The semantic analysis yielded 15 valid **Generic Non-Zeroing (`kmalloc-192`)** candidates:
- `audit_aux_data_bprm_fcaps`
- `rhltable`
- `elf_prpsinfo`
- `nfs_open_context`
- `p9_client`
- `io_futex_data`
- `uart_8250_em485`
- `ports_device`
- `netpoll_info`
- `ptr_ring`
- `log_c`
- `snd_pcm_sw_params`
- `snd_seq_port_info`
- `nfnl_err`
- `xfrm_userpolicy_info`

## 3. Offset 160 Layout Analysis
We ran `pahole -F dwarf` against these 15 candidates to map exactly what resides at offset 160:
- **`nfs_open_context`**: Offset 160 is `callback_head.func` (function pointer, 8 bytes).
- **`netpoll_info`**: Offset 160 is `rcu.func` (function pointer, 8 bytes).
- **`log_c`**: Offset 160 is `header_location.bdev` (block device pointer, 8 bytes).
- **`ports_device`**: Offset 160 is `chr_major` (integer, 4 bytes).
- **`xfrm_userpolicy_info`**: Offset 160 is `dir` (integer, 1 byte).
- **`nfnl_err`**: Offset 160 falls inside the `extack.cookie` array.
- **Others**: Offset 160 is structure padding or end-of-struct space.

## 4. Final Exploitability Assessment
The `eventpoll` Use-After-Free primitive forces an 8-byte write of exactly `0x0000000000000000` at offset 160 of the reclaimed struct.

Applying this primitive to the best surviving targets:
- If we spray `nfs_open_context` or `netpoll_info`, we overwrite a `func` pointer with `NULL`. When the kernel subsequently attempts to execute this callback, it will dereference the `NULL` pointer.
- If we spray `log_c`, we overwrite the `bdev` pointer with `NULL`. When the DM subsystem processes this I/O region, it will dereference the `NULL` pointer.

Because `mmap_min_addr` (default 65536) mitigations have been standard in the Linux kernel for over a decade, user-space cannot map address `0x0`. Therefore, a forced `NULL` pointer dereference cannot be hijacked to execute an arbitrary user-space payload. It will invariably result in an unrecoverable kernel panic.

**Conclusion:** The semantic search confirmed that while viable generic `kmalloc-192` targets exist, none possess fields at offset 160 that can translate an 8-byte `NULL` write into arbitrary control-flow hijack or memory manipulation. On this specific kernel build, the vulnerability is strictly constrained to a **Local Denial of Service (LDoS)**. It is practically unexploitable for Privilege Escalation (LPE).
