**Just published a new deep dive on Medium! 🚀**

Over the past few weeks, I’ve been conducting an intensive engineering deep-dive into **CVE-2026-46242**, commonly known as "Bad Epoll." This kernel vulnerability is a race condition leading to a Use-After-Free in the Linux event polling subsystem.

While I successfully reproduced a highly reliable root exploit on an x86_64 Linux environment (Tier 1), porting it to an Android ARM64 Generic Kernel Image (Tier 2) was a completely different beast. 

My new post explores:
✅ The mechanics of the `close-vs-close` race condition.
✅ The elegant x86_64 exploit chain (Cross-Cache, KASLR leak, ROP).
🛡️ Why Android GKI mitigations like **kCFI, PAC, BTI, and slab isolation** stopped the exploit dead in its tracks.
🔍 How we arrived at a scientifically defensible **DoS-only verdict** after 19 experiments and 21 dead ends.
➡️ The pivot to our next target: **vendor GPU driver primitives (Arm vendor GPU driver UAF)**.

Kernel exploitation on Android is evolving rapidly. Hardware and software mitigations are fundamentally shifting the attack surface from core kernel components to vendor drivers.

Read the full technical breakdown here: [Link to Medium Article]

I’d love to hear thoughts from the security research and Android engineering communities! 

#CyberSecurity #KernelExploitation #AndroidSecurity #VulnerabilityResearch #CVE202646242 #BadEpoll #LinuxKernel #ARM64 #InfoSec
