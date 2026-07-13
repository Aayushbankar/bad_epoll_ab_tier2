# Kernel Exploit Porting Workflow & CVE-2026-46242 FAQ

This document outlines a structured, 8-phase workflow for porting public kernel exploits (like kernelCTF submissions) to custom or updated targets. It also provides a core FAQ for CVE-2026-46242 (Bad Epoll) to serve as a baseline understanding before applying the workflow.

## Part 1: CVE-2026-46242 (Bad Epoll) Core FAQ

*   **What is the CVE?**
    CVE-2026-46242, commonly known as "Bad Epoll".
*   **What kernel subsystem is affected?**
    The `epoll` subsystem (`fs/eventpoll.c`).
*   **Where does the UAF occur?**
    A race condition occurs between `__ep_remove()` and the lockless fast-path in `eventpoll_release()`. A concurrent close operation allows `eventpoll_release` to see a NULL pointer and skip cleanup, leading to the memory being freed while `__ep_remove` still holds a reference.
*   **What object is freed?**
    Initially, a `struct eventpoll` (kmalloc-192). Ultimately, this leads to a dangling pointer to a freed `struct file`.
*   **What object replaces it?**
    The exploit uses a **Cross-Cache Attack** to reclaim the freed `struct file`'s slab page as a **pipe buffer page** (`pipe_buffer`), giving the attacker arbitrary write control over the forged file object.
*   **How is AAR (Arbitrary Address Read) achieved?**
    By controlling the forged `struct file`, the attacker sets a fake `f_inode` pointer. When reading `/proc/self/fdinfo/`, the kernel calls `ep_show_fdinfo()`, which dereferences the fake inode and leaks kernel memory contents.
*   **How is RIP controlled?**
    The attacker sets the forged `file->f_op` (file operations pointer) to point to user-controlled memory (or the pipe buffer). When `epoll_wait()` is called on the UAF file, the kernel invokes `file->f_op->poll()`, hijacking the instruction pointer (RIP).
*   **Why is a ROP chain needed?**
    Modern kernels employ SMEP (Supervisor Mode Execution Prevention) and SMAP (Supervisor Mode Access Prevention). You cannot simply jump to shellcode in userspace. A ROP (Return-Oriented Programming) chain is required to stitch together existing kernel code snippets (gadgets) to safely elevate privileges and disable protections.
*   **What should happen after `commit_creds()`?**
    After `commit_creds(&init_cred)` elevates the process to `uid=0`, the ROP chain must call `switch_task_namespaces()` to escape any namespace isolation, cleanly restore the kernel stack context (KPTI trampoline), and execute a safe return to userspace (e.g., `swapgs; iretq`) to spawn `/bin/sh`.

---

## Part 2: Structured Workflow for Porting a Public Kernel Exploit

A structured workflow for porting a public kernel exploit (such as from kernelCTF) to a new target environment.

### Phase 1 — Understand the exploit

Before changing anything, answer:
*   What vulnerability is exploited?
*   What primitives are required?
*   Which kernel objects are corrupted?
*   Which mitigation is bypassed at each stage?
*   Where does execution finally transfer?

**Draw the exploit as a pipeline:**
```text
Race
  ↓
UAF
  ↓
Cross-cache reclaim
  ↓
Arbitrary Read
  ↓
Find current task
  ↓
Locate cred
  ↓
Overwrite function pointer
  ↓
ROP
  ↓
commit_creds()
```

### Phase 2 — Compare environments

Create a checklist to identify differences between the PoC's assumed environment and your target.

| Item | Original | Yours |
| :--- | :--- | :--- |
| Kernel version | ✓ | ✓ |
| Kernel config | ? | ✓ |
| Compiler | ? | GCC 16 |
| Gadget addresses | Original | Unknown |
| `task_struct` offsets | Original | Different |
| CPU flags | Original | Different |
| Virtualization | ? | QEMU (`kvm64`) |

Every difference becomes something to investigate.

### Phase 3 — Verify assumptions

Instead of trusting the exploit, question every constant and assumption:
*   **Where did this value come from?**
    For example: `0xffffffff820d6525`
    Don't assume it's correct. Determine:
    *   Is it inside `.text`?
    *   Which instruction is actually there?
    *   Is it still a gadget?
    *   Does it still end in `ret`?

### Phase 4 — Rebuild the target database

Create your own notes and maps for your specific kernel build:
*   Kernel Base Address
*   Task offsets (`task_struct`)
*   Cred offsets
*   ROP gadgets
*   Interesting functions (`init_task`, `commit_creds`)
*   Pipe buffer layout

Think of this as documentation for your specific kernel environment.

### Phase 5 — Validate each primitive independently

Don't run the full exploit every time. Test individually.
*   Can I win the race? → Yes. Next.
*   Can I trigger UAF? → Yes. Next.
*   Can I reclaim? → Yes. Next.
*   Can I read kernel memory? → Yes. Next.

Continue until one primitive fails. That primitive becomes today's task.

### Phase 6 — Reverse engineer instead of patching

When something fails, don't modify code immediately. Instead, answer:
*   Why?
*   Which assumption failed?
*   Which register has the wrong value?
*   Which pointer is invalid?
*   Which instruction faulted?

*Only then edit code.*

### Phase 7 — Instrument heavily

Use the right tools to collect evidence before changing anything:
*   `GDB`
*   `objdump`
*   `nm`
*   `readelf`
*   `pahole`
*   `ROPgadget`
*   `dmesg`

### Phase 8 — Record everything

Maintain tables tracking your alignment efforts:

| Target | Old Value | New Value | Reason |
| :--- | :--- | :--- | :--- |
| `task_struct->comm` | 1928 | 1840 | Different config. Verified with GDB. |
| `PIVOT1` Gadget | `0xffffffff8xxxxxxx` | `0xffffffff820d6525` | Replaced JOP with direct pivot. |

Do the same for all gadgets and offsets.
