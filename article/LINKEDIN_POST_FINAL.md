# LinkedIn Post — CVE-2026-46242 Deep Dive Launch

---

**What happens when a high-reliability Linux kernel exploit meets Android's mitigation stack?**

I just published a technical deep-dive into **CVE-2026-46242 ("Bad Epoll")** — a Use-After-Free race condition in the Linux epoll subsystem originally discovered and exploited by Jaeyoung Chung via Google kernelCTF (~99% reliable root exploit on x86_64).

My research investigated whether this primitive could achieve privilege escalation on modern mobile targets:
🔹 **Tier 1 (x86_64 Linux VM):** Independently reproduced and ported the original exploit chain to verify the primitive (UID 0, KASLR bypass, ROP).
🔹 **Tier 2 (ARM64 Android 14 GKI):** Original exploitability research testing the primitive against Android's hardened kernel — where we documented 21 dead ends, 0/102,740 natural race hits, and established a scientifically defensible **DoS-only verdict**.

The writeup breaks down the slab cache mechanics, `msg_msg` heap sprays, why `hlist_del_rcu` writes a fixed NULL at offset 160, and how PAC + kCFI + BTI + slab isolation collectively neutralized every escalation path.

**One question I'm still turning over:** is there a `kmalloc-192` struct on GKI 6.1 where zeroing 8 bytes at offset 160 yields more than a kernel panic? EXP-016 says no, but if you've done slab cross-referencing on recent GKI builds, I'd value your perspective.

Full technical writeup with evidence ledgers, repo links, and architecture diagrams: [Link to Medium Article]

#LinuxKernel #ExploitDev #AndroidSecurity #CVE #InfoSec #KernelExploitation #VulnerabilityResearch #ARM64 #OffensiveSecurity

---
