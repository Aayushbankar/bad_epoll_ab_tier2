# Subsystem Dependency Map

This document visually maps the complex relationships between the kernel subsystems relevant to Android GKI local privilege escalation.

## Component Interaction Diagram

```mermaid
graph TD
    %% Core Entities
    Userspace[Userspace Exploit]
    Syscall[Syscall Interface]
    VFS[Virtual File System]
    Epoll[Eventpoll Subsystem]
    Sched[Scheduler & Wait Queues]
    SLUB[SLUB Allocator]
    
    %% Target Payloads
    MsgMsg[msg_msg]
    Pipe[pipe_buffer]
    Binder[Android Binder IPC]
    
    %% Security Boundaries
    SELinux[SELinux Hooks]
    PAC_BTI[PAC / BTI Enforcements]
    
    %% Relationships
    Userspace -->|Triggers| Syscall
    Syscall --> VFS
    
    VFS -->|File Operations| Epoll
    VFS -->|Pipes| Pipe
    VFS -->|Character Devices| Binder
    
    Epoll -->|Registers Callbacks| Sched
    Epoll -->|Allocates epitem| SLUB
    
    Sched -->|Triggers Wakeups| Epoll
    Sched -->|Preemption| Userspace
    
    MsgMsg -->|Allocates Message Segments| SLUB
    Pipe -->|Allocates pipe_buffer| SLUB
    Binder -->|Allocates binder_node| SLUB
    
    %% Security Interventions
    SELinux -.->|Intercepts| VFS
    SELinux -.->|Intercepts| Binder
    PAC_BTI -.->|Protects Pointers| VFS
    PAC_BTI -.->|Protects Pointers| Epoll
    PAC_BTI -.->|Protects Pointers| Sched

    classDef core fill:#f9f,stroke:#333,stroke-width:2px;
    classDef target fill:#bbf,stroke:#333,stroke-width:2px;
    classDef security fill:#fbb,stroke:#333,stroke-width:2px;
    
    class Epoll,VFS,Sched,SLUB core;
    class MsgMsg,Pipe,Binder target;
    class SELinux,PAC_BTI security;
```

## Description of Interactions
1. **Epoll -> SLUB -> Pipe/MsgMsg/Binder**: The core UAF occurs in `Epoll`. It is managed by `SLUB`. The exploit relies on using `Pipe`, `MsgMsg`, or `Binder` to spray the `SLUB` allocator and overlap the freed `Epoll` memory.
2. **Epoll -> VFS -> PAC/BTI**: `Epoll` calls into `VFS` via `f_op->poll()`. The `VFS` function pointers are heavily protected by `PAC/BTI`, requiring bypass mechanisms.
3. **Userspace -> Binder -> SELinux**: The userspace process interacts with `Binder`, but the resulting transactions and file descriptors are policed by `SELinux`.
