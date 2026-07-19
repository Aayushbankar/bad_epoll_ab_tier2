# Expanded Kernel Inventory (Android 14 GKI)

This document indexes the critical kernel subsystems required for Android GKI exploitation research.

## 1. epoll Subsystem
- **Purpose**: Provides scalable I/O event notification mechanisms.
- **Source Files**: `fs/eventpoll.c`, `include/linux/eventpoll.h`
- **Important Structures**: `struct eventpoll`, `struct epitem`, `struct eppoll_entry`
- **Important Functions**: `ep_insert()`, `ep_remove()`, `ep_poll()`, `epoll_ctl()`
- **Call Graph Entry Points**: `sys_epoll_ctl()`, `sys_epoll_wait()`
- **Locking Primitives**: `ep->lock` (rwlock), `ep->wq.lock` (spinlock)
- **Memory Allocation APIs**: `kmem_cache_alloc()` from `epi_cache` and `pwq_cache`.
- **Related Subsystems**: VFS, Scheduler (Wait Queues)
- **Notes**: Root cause of CVE-2026-46242 (UAF on `epitem`).
- **References**: `fs/eventpoll.c` source code.

## 2. Scheduler & Wait Queues
- **Purpose**: Manages thread execution, preemption, and asynchronous blocking.
- **Source Files**: `kernel/sched/core.c`, `include/linux/wait.h`, `kernel/sched/wait.c`
- **Important Structures**: `struct rq`, `struct task_struct`, `wait_queue_head_t`, `wait_queue_entry_t`
- **Important Functions**: `schedule()`, `wake_up()`, `prepare_to_wait()`
- **Call Graph Entry Points**: Core interrupt returns, `sys_sched_yield()`
- **Locking Primitives**: Runqueue spinlocks, `wait_queue_head->lock`.
- **Memory Allocation APIs**: Core kernel bootmem.
- **Related Subsystems**: All blocking I/O (epoll, VFS).
- **Notes**: Essential for triggering and surviving the race condition.
- **References**: `kernel/sched/`

## 3. Credentials (cred)
- **Purpose**: Tracks process privileges and capabilities.
- **Source Files**: `kernel/cred.c`, `include/linux/cred.h`
- **Important Structures**: `struct cred`
- **Important Functions**: `commit_creds()`, `prepare_creds()`
- **Call Graph Entry Points**: `sys_setuid()`, `sys_capset()`
- **Locking Primitives**: RCU (Read-Copy-Update) for cred pointers.
- **Memory Allocation APIs**: `kmem_cache_alloc()` from `cred_jar`.
- **Related Subsystems**: `task_struct`, SELinux.
- **Notes**: Target object for arbitrary write LPE.
- **References**: `kernel/cred.c`

## 4. task_struct
- **Purpose**: Represents a thread/process in the kernel.
- **Source Files**: `include/linux/sched.h`
- **Important Structures**: `struct task_struct`, `struct thread_info`
- **Important Functions**: `copy_process()`, `find_task_by_vpid()`
- **Call Graph Entry Points**: `sys_clone()`, `sys_fork()`
- **Locking Primitives**: `task_lock()`, RCU.
- **Memory Allocation APIs**: `kmem_cache_alloc()` from `task_struct` cache.
- **Related Subsystems**: Scheduler, Credentials, namespaces.
- **Notes**: Useful for leaking pointers to `cred`.
- **References**: `include/linux/sched.h`

## 5. SLUB Allocator
- **Purpose**: The default kernel slab allocator.
- **Source Files**: `mm/slub.c`, `include/linux/slub_def.h`
- **Important Structures**: `struct kmem_cache`, `struct slab`, `struct page`
- **Important Functions**: `kmem_cache_alloc()`, `kfree()`
- **Call Graph Entry Points**: All kernel allocations.
- **Locking Primitives**: Per-CPU lockless fastpaths, node spinlocks.
- **Memory Allocation APIs**: Page allocator.
- **Related Subsystems**: Memory Management.
- **Notes**: Hardened via `CONFIG_SLAB_FREELIST_RANDOM` and `CONFIG_SLAB_FREELIST_HARDENED`.
- **References**: `mm/slub.c`

## 6. VFS (file_operations)
- **Purpose**: Virtual File System abstraction layer.
- **Source Files**: `fs/read_write.c`, `include/linux/fs.h`
- **Important Structures**: `struct file`, `struct file_operations`, `struct inode`
- **Important Functions**: `vfs_read()`, `vfs_write()`, `fput()`
- **Call Graph Entry Points**: `sys_read()`, `sys_write()`, `sys_close()`
- **Locking Primitives**: RCU, inode semaphores.
- **Memory Allocation APIs**: `filp` cache.
- **Related Subsystems**: Epoll, underlying filesystems.
- **Notes**: `f_op->poll` is protected by PAC.
- **References**: `fs/read_write.c`

## 7. pipe_buffer
- **Purpose**: Implements anonymous pipes.
- **Source Files**: `fs/pipe.c`, `include/linux/pipe_fs_i.h`
- **Important Structures**: `struct pipe_inode_info`, `struct pipe_buffer`
- **Important Functions**: `pipe_write()`, `pipe_read()`
- **Call Graph Entry Points**: `sys_pipe()`
- **Locking Primitives**: Mutexes.
- **Memory Allocation APIs**: `kmalloc` array for buffers.
- **Related Subsystems**: VFS.
- **Notes**: Classic exploit spray primitive.
- **References**: `fs/pipe.c`

## 8. msg_msg
- **Purpose**: System V Message Queues.
- **Source Files**: `ipc/msgutil.c`, `ipc/msg.c`
- **Important Structures**: `struct msg_msg`, `struct msg_msgseg`
- **Important Functions**: `load_msg()`, `free_msg()`
- **Call Graph Entry Points**: `sys_msgsnd()`, `sys_msgrcv()`
- **Locking Primitives**: IPC locks.
- **Memory Allocation APIs**: `kmalloc()` for primary, `kmalloc()` for segments.
- **Related Subsystems**: IPC.
- **Notes**: Highly flexible spray primitive due to arbitrary allocation sizes.
- **References**: `ipc/msgutil.c`

## 9. Security Subsystem & SELinux
- **Purpose**: Provides Mandatory Access Control and security hooks.
- **Source Files**: `security/security.c`, `security/selinux/hooks.c`
- **Important Structures**: `struct task_security_struct`, `struct cred->security`
- **Important Functions**: `security_file_open()`, `selinux_cred_alloc_blank()`
- **Call Graph Entry Points**: Triggered transparently on almost all syscalls via LSM hooks.
- **Locking Primitives**: RCU.
- **Memory Allocation APIs**: Dedicated caches for SIDs.
- **Related Subsystems**: VFS, Credentials, Networking.
- **Notes**: Blocks `uid=0` operations unless the SELinux domain is also bypassed.
- **References**: `security/selinux/`

## 10. Android Binder IPC
- **Purpose**: The fundamental Android RPC mechanism.
- **Source Files**: `drivers/android/binder.c`, `drivers/android/binder_alloc.c`
- **Important Structures**: `struct binder_proc`, `struct binder_thread`, `struct binder_transaction`
- **Important Functions**: `binder_ioctl()`, `binder_transaction()`
- **Call Graph Entry Points**: `ioctl(BINDER_WRITE_READ)`
- **Locking Primitives**: Inner and outer spinlocks, global mutexes.
- **Memory Allocation APIs**: `kmalloc`, direct `vmap` page mapping.
- **Related Subsystems**: SELinux, VFS.
- **Notes**: Powerful attack surface and heap manipulation mechanism.
- **References**: `drivers/android/binder.c`
