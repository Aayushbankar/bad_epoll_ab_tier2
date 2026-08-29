# LinkedIn Post — CVE-2026-46242 Deep Dive Launch

---

**99% reliable Linux root exploit — yet on Android it's DoS-only.** I ported CVE-2026-46242 ("Bad Epoll", found by Jaeyoung Chung via Google kernelCTF) to ARM64 Android GKI 6.1 and hit 0/102,740 natural race hits and 21 documented dead ends. Here's why PAC, kCFI, BTI, and slab isolation held.

My research asked whether this primitive escalates on hardened mobile kernels:
🔹 **Tier 1 (x86_64 Linux VM):** reproduced the original kernelCTF chain to a working root shell (UID 0, KASLR bypass, ROP).
🔹 **Tier 2 (ARM64 Android 14 GKI):** original exploitability research — 21 dead ends, 0/102,740 natural race hits, a scientifically defensible **DoS-only verdict**.

The writeup breaks down slab-cache mechanics, `msg_msg` heap sprays, why `hlist_del_rcu` writes a fixed NULL at offset 160, and how PAC + kCFI + BTI + slab isolation collectively neutralized every escalation path.

**One open question:** is there a `kmalloc-192` struct on GKI 6.1 where zeroing 8 bytes at offset 160 yields more than a kernel panic? EXP-016 says no — if you've done slab cross-referencing on recent GKI builds, I'd value your perspective.

Full technical writeup with evidence ledgers, repo links, and architecture diagrams: <<MEDIUM_URL_PENDING>>

#LinuxKernel #ExploitDev #AndroidSecurity #CVE #InfoSec #KernelExploitation #VulnerabilityResearch #ARM64 #OffensiveSecurity
