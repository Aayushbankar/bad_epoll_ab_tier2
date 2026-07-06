# Article Draft: Recreating "Bad Epoll" — From CVE to Root Shell on Android

> **Status:** Not started — will be written after completing Tier 1-3
> **Target platforms:** Medium, LinkedIn
> **Tone:** Technical + Accessible hybrid
> **Author:** Ayush (CypherMatrix)

---

## Outline

### 1. Hook
*"A single race condition in 10 instructions can root every modern device running Linux kernel 6.4+. I recreated the exploit from scratch. Here's what I learned."*

### 2. What is "Bad Epoll"?
- Plain English explanation of epoll (the hotel receptionist analogy)
- What goes wrong: the race condition
- Why it matters: unprivileged → root in seconds

### 3. The Technical Deep Dive
- The race window and how it's widened (false sharing, timerfd)
- UAF → Cross-cache → Arbitrary read → RIP control
- Code snippets from the actual exploit
- Diagrams showing the memory corruption flow

### 4. My Hands-On Experience
- Setting up the vulnerable kernel in QEMU
- Compiling and running the exploit
- What worked, what broke, how I debugged it
- Screenshots and terminal output

### 5. The Android Reality
- Which Android versions are affected (kernel 6.6+ only)
- Why uid=0 ≠ full Android root (SELinux)
- What a real attacker would still need to do
- The gap between "kernel exploitation" and "device compromise"

### 6. What This Means
- For security researchers: exploit chain collapse is real
- For developers: why kernel updates matter
- For users: why you should keep your phone updated

### 7. Credits & Timeline
- Original researchers (J-jaeyoung, kernelCTF)
- Responsible disclosure timeline
- Links to the fix

---

## Notes for Writing

- Include actual terminal screenshots from Tier 1 and Tier 2
- Show the exploit output (root shell moment)
- Include the mermaid diagram of the exploit chain from README
- Keep paragraphs short for Medium readability
- Add a TL;DR at the top for LinkedIn
