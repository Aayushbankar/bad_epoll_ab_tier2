# Exploit Architecture

## Core Exploit Component: `exploit.cpp`
The core exploit relies on `exploit.cpp` to abuse `CVE-2026-46242`, the "Bad Epoll" vulnerability. The vulnerability triggers a Use-After-Free (UAF) in the Linux kernel `epoll` subsystem due to an incorrect RCU lifecycle within specific file descriptor closure conditions.

### Control Flow
1. **Setup & Initialization**: Registers signals and prepares the virtual environment mapping via `libxdk`.
2. **KASLR Leak & AAR**: Overwrites `epoll` queues with user-controlled sizes, allowing arbitrary heap allocations to achieve an Arbitrary Address Read (AAR). The leak bypasses KASLR by locating the active `task_struct` and computing `kernel_base`.
3. **Payload Injection**: Maps a physical userspace page `payload page (virt+0x120)` housing the dynamically generated ROP chain payload.
4. **JOP Bridge (The Stack Pivot)**: Exploits the UAF function pointer (specifically `f_op->poll`) to hijack execution flow. The RIP control relies on 4 sequential pivots (PIVOT1 through PIVOT4) connected via `__x86_indirect_call_thunk_rdi` targeting gadgets that progressively adjust CPU state and exchange `RSP` to point directly to the user-supplied payload page.
5. **Execution**: The ROP chain executes `commit_creds(prepare_kernel_cred(&init_task))`, switches namespaces, performs a KPTI-compatible `SWAPGS_RESTORE_REGS_AND_RETURN_TO_USERMODE`, and vectors back into the userspace function `rip_post_exploit`.

## Supporting Framework: `libxdk`
`libxdk` (Kernel eXploit Development Kit) serves as the abstract interaction layer between the exploit and the specific kernel layout. It parses `.kxdb` databases to completely decouple raw hex offsets from the core C++ codebase. It uses ULEB128 serialization to maintain extreme efficiency. 

## Target Profile & Generator Pipeline
The database structure relies heavily on dynamic evaluation logic inside `image_db`. Rather than patching `exploit.cpp` directly for every new kernel, researchers use the pipeline scripts (`angrop_rop_generator.py`, `extract_structures.py`) to scrape DWARF symbols and brute-force `vmlinux` code blocks with `angrop` and `rp++`. 
The results are compressed via `kxdb_tool` into `target_db.kxdb`, which `libxdk` consumes at runtime.
