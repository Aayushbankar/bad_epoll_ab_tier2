# LinkedIn Post — CVE-2026-46242 Deep Dive Launch

---

**99% reliable Linux root exploit — yet on a backported-vulnerable Android build it's DoS-only.** I ported CVE-2026-46242 ("Bad Epoll", found by Jaeyoung Chung via Google kernelCTF) to a *deliberately backported-vulnerable* ARM64 Android GKI 6.1 build (stock GKI 6.1 is **not** affected — the bug was introduced in v6.4) and hit 0/102,740 natural race hits and 21 documented dead ends. Here's why PAC, kCFI, BTI, and slab isolation held.

My research asked whether this primitive *would* escalate on hardened mobile kernels (tested on that synthetic vulnerable GKI 6.1 build):
🔹 **Tier 1 (x86_64 Linux VM):** reproduced the original kernelCTF chain to a working root shell (UID 0, KASLR bypass, ROP).
🔹 **Tier 2 (ARM64 Android 14 GKI, backported-vulnerable build):** original exploitability research — 21 dead ends, 0/102,740 natural race hits, a scientifically defensible **DoS-only verdict** (a portability/mitigation study, *not* a claim about production Android).

The writeup breaks down slab-cache mechanics, `msg_msg` heap sprays, why `hlist_del_rcu` writes a fixed NULL at offset 160, and how PAC + kCFI + BTI + slab isolation collectively neutralized every escalation path.

**One open question:** is there a `kmalloc-192` struct on (vulnerable) GKI 6.1 where zeroing 8 bytes at offset 160 yields more than a kernel panic? EXP-016 says no — if you've done slab cross-referencing on recent GKI builds, I'd value your perspective.

Medium article (distilled, technical): <<MEDIUM_URL_PENDING>>

📄 Complete 26-page technical writeup (PDF) — executive summary, Tier-1 offsets/gadgets, 21 dead ends with killing evidence, experiment index, and verification ledger excerpt — available on GitHub (see Medium § Links): https://github.com/Aayushbankar/bad-epoll-lab/blob/publish/clean-and-writeup-2026-08-29/article/CVE-2026-46242_Technical_Writeup.pdf — ping for direct download.

#LinuxKernel #ExploitDev #AndroidSecurity #CVE #InfoSec #KernelExploitation #VulnerabilityResearch #ARM64 #OffensiveSecurity
