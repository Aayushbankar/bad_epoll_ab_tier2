# Kernel Execution Flow (CVE-2026-46242 context)

This document traces the complete execution path from a userspace system call to the internal kernel subsystems and back, specifically focusing on the `epoll` and VFS interactions critical for triggering the vulnerability.

## Execution Path Diagram

```mermaid
sequenceDiagram
    participant User as Userspace Process
    participant Syscall as Syscall Interface (arm64/kernel/syscall.c)
    participant VFS as Virtual File System (fs/read_write.c)
    participant Epoll as Eventpoll Subsystem (fs/eventpoll.c)
    participant Sched as Scheduler (kernel/sched/core.c)

    User->>Syscall: epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event)
    activate Syscall
    Syscall->>VFS: fdget() (Resolve file descriptors)
    activate VFS
    VFS->>Epoll: ep_insert()
    activate Epoll
    
    Note over Epoll: Allocates epitem from slab<br/>Links epitem to epoll RB-tree<br/>Links epitem to target file wait queue
    
    Epoll->>VFS: f_op->poll()
    VFS-->>Epoll: Return poll mask
    
    Epoll-->>VFS: Return 0 (Success)
    deactivate Epoll
    VFS-->>Syscall: fdput()
    deactivate VFS
    Syscall-->>User: Return 0
    deactivate Syscall

    %% Race Condition Trigger Path
    par Thread 1
        User->>Syscall: close(fd)
        activate Syscall
        Syscall->>VFS: filp_close() -> fput()
        activate VFS
        VFS->>Epoll: eventpoll_release_file()
        activate Epoll
        Note over Epoll: Traverses linked list<br/>Calls ep_remove()
        Epoll->>Sched: spin_lock_irqsave() (Wait Queue Lock)
        Sched-->>Epoll: Lock Acquired
        Note over Epoll: Frees epitem (kmem_cache_free)
        Epoll-->>VFS: Return
        deactivate Epoll
        VFS-->>Syscall: Return
        deactivate VFS
        Syscall-->>User: Return 0
        deactivate Syscall
    and Thread 2
        User->>Syscall: epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &event)
        activate Syscall
        Syscall->>VFS: fdget()
        activate VFS
        VFS->>Epoll: ep_find()
        activate Epoll
        Note over Epoll: Finds epitem in RB-tree<br/>(Race: Thread 1 frees this epitem)
        Epoll->>VFS: f_op->poll() on FREED epitem
        Note over Epoll: **USE-AFTER-FREE TRIGGERED**
        deactivate Epoll
        deactivate VFS
        deactivate Syscall
    end
```
