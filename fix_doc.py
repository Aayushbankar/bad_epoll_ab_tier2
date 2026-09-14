import re

with open("tier2/docs/PHASE3_QEMU_POC_PROPOSAL.md", "r") as f:
    content = f.read()

# Restore the ASCII block
ascii_block = """│  Stage 1: Survivor Creation (GDB-forced on QEMU)               │
│  ┌──────────────────────────────────────────────────────────┐   │
│  │ Thread A: close(outer_epoll)                             │   │
│  │   → __ep_remove(ep, epi)                                 │   │
│  │   → loads epi->ffd.file (unpinned, no epi_fget)          │   │
│  │   → WRITE_ONCE(file->f_ep, NULL) at +224                 │   │
│  │   → [RACE WINDOW: 11 instructions on 6.6]               │   │
│  │   → hlist_del_rcu(&epi->fllink) writes NULL to +160      │   │
│  │                                                          │   │
│  │ Thread B: close(inner_file)                              │   │
│  │   → __fput → eventpoll_release(file)                     │   │
│  │   → sees f_ep == NULL (fast-path bypass!)                │   │
│  │   → skips eventpoll_release_file()                       │   │
│  │   → frees inner struct eventpoll to kmalloc-192          │   │
│  └──────────────────────────────────────────────────────────┘   │
│  Result: inner_epoll->refs.first = NULL                         │
│                                                                 │
│  Stage 1b: Close corrupted inner_epoll                          │"""

content = content.replace("│  Stage 1b: Close corrupted inner_epoll                          │", ascii_block, 1)

# Replace flowchart
old_mermaid = """```mermaid
flowchart TD
    subgraph Stage 1
        RACE[__ep_remove race win] --> NULL_WRITE[UAF NULL write to refs.first]
        NULL_WRITE --> SURVIVOR[Dangling Survivor epitem]
    end
    
    subgraph Stage 2
        SURVIVOR --> DRAIN[memfd drain protocol]
        DRAIN --> SPRAY[dma_buf page spray]
    end
    
    subgraph Stage 3
        SPRAY --> FDINFO[fdinfo Oracle check]
        FDINFO --> |Miss| DRAIN
        FDINFO --> |Hit| WRITE[swaps_poll 32-bit zero]
        WRITE --> ROOT[uid=0, euid=0, fsuid=0]
    end
```"""

new_mermaid = """```mermaid
flowchart TD
    subgraph Stage 1: Survivor Creation
        direction LR
        GDB[GDB Variant B<br>Direct f_ep NULL write<br>refs.first UNTOUCHED<br><b>EXP-032 uses this</b>]
        NAT[Natural Race<br>UAF write NULL to refs.first]
        
        GDB --> SURV[Dangling Survivor epitem]
        NAT --> SURV
    end
    
    subgraph Stage 2: Reclaim
        SURV --> DRAIN[memfd drain protocol]
        DRAIN --> SPRAY[dma_buf page spray]
    end
    
    subgraph Stage 3: AAR & Write
        SPRAY --> FDINFO[fdinfo Oracle check]
        FDINFO --> |Miss| DRAIN
        FDINFO --> |Hit| WRITE[swaps_poll 32-bit zero]
        WRITE --> ROOT[uid=0, euid=0, fsuid=0]
    end
```"""

content = content.replace(old_mermaid, new_mermaid)

with open("tier2/docs/PHASE3_QEMU_POC_PROPOSAL.md", "w") as f:
    f.write(content)

