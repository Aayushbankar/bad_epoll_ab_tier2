# LinkedIn Post — CVE-2026-46242 Deep Dive Launch

---

**What happens when a root exploit meets Android's mitigation stack?**

I just published a deep-dive into CVE-2026-46242 ("Bad Epoll") — a Use-After-Free race condition in the Linux kernel's epoll subsystem. On x86_64, I turned it into a working root exploit (UID 0, KASLR bypassed, ROP chain executing `commit_creds`). On ARM64 Android GKI — 0 natural race hits in 102,740 attempts, 21 documented dead ends, and a final verdict of **DoS-only**.

The post covers the full chain: slab cache mechanics, `msg_msg` heap spray, why `hlist_del_rcu` writes NULL at offset 160, and how PAC + kCFI + BTI + slab isolation collectively killed every escalation path.

**One question I'm still turning over:** is there a `kmalloc-192` struct on GKI 6.1 where zeroing 8 bytes at offset 160 yields more than a kernel panic? The EXP-016 audit says no, but the slab landscape is deep. If you've done cross-cache analysis on recent GKI builds, I'd genuinely like to hear your take.

Full technical writeup with evidence chain, repo links, and Mermaid diagrams: [Link to Medium Article]

#LinuxKernel #ExploitDev #AndroidSecurity #CVE #InfoSec #KernelExploitation #VulnerabilityResearch #ARM64 #OffensiveSecurity

---
