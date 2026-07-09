#!/usr/bin/env python3
"""
find_real_pivot.py — Finds instruction-boundary-aligned stack pivot gadgets
in a vmlinux binary using objdump -M intel output.

ROPgadget finds byte-sequence gadgets that cross instruction boundaries,
which are NOT usable in kernel ROP. This script only finds gadgets that
START at a real instruction boundary.

Usage:
    python3 find_real_pivot.py [vmlinux_path]
"""
import sys
import subprocess
import re
from collections import deque

VMLINUX = sys.argv[1] if len(sys.argv) > 1 else "linux-6.12.67/vmlinux"

# Patterns for instructions we care about
PIVOT_PATTERNS = [
    # Tier 1: direct 1-gadget pivots (rdi or rdx into rsp)
    re.compile(r'mov\s+rsp,\s*rdi$'),
    re.compile(r'mov\s+rsp,\s*rdx$'),
    re.compile(r'xchg\s+rsp,\s*rdi$'),
    re.compile(r'xchg\s+rdi,\s*rsp$'),
    re.compile(r'xchg\s+rsp,\s*rdx$'),
    re.compile(r'xchg\s+rdx,\s*rsp$'),
]

# For 2-gadget search: "push rdi/rdx" then "pop rsp" then "ret"
PUSH_RDI = re.compile(r'push\s+rdi$')
PUSH_RDX = re.compile(r'push\s+rdx$')
POP_RSP  = re.compile(r'pop\s+rsp$')
RET      = re.compile(r'^ret$')

# For POP_RDI_RET
POP_RDI  = re.compile(r'pop\s+rdi$')

# For JOP chain PIVOT1: mov rax,[rdx+OFFSET] ; <optional> ; jmp rax
MOV_RAX_MEM_RDX = re.compile(r'mov\s+rax,QWORD PTR \[rdx\+0x[0-9a-f]+\]$')
JMP_RAX         = re.compile(r'jmp\s+rax$')

print("[*] Disassembling vmlinux .text section (this may take ~60s)...")
print(f"[*] Binary: {VMLINUX}")
print()

# Run objdump on just the .text section
proc = subprocess.Popen(
    ['objdump', '-d', '-M', 'intel',
     '--section=.text',
     VMLINUX],
    stdout=subprocess.PIPE,
    stderr=subprocess.DEVNULL,
    text=True
)

addr_re = re.compile(r'^([0-9a-f]{16}):\s+(?:[0-9a-f]{2} )+\s+(.+)$')

# Sliding window: keep last 4 instructions
window = deque(maxlen=4)

results_1gadget = []
results_push_pop = []   # (push_addr, push_insn, pop_addr, ret_addr)
results_pop_rdi  = []
results_jop      = []   # (jop1_addr, jop2_addr)

jop_candidate = None    # (addr, insn) where insn matched MOV_RAX_MEM_RDX

line_count = 0
for line in proc.stdout:
    line_count += 1
    line = line.strip()
    m = addr_re.match(line)
    if not m:
        window.clear()
        jop_candidate = None
        continue

    addr_str, insn = m.group(1), m.group(2).strip()
    addr = int(addr_str, 16)

    # ── 1-gadget pivot search ──────────────────────────────────────────────
    for pat in PIVOT_PATTERNS:
        if pat.match(insn):
            # Check next instruction is ret (peek — we'll catch it in next iter)
            results_1gadget.append((addr, insn, "PIVOT_PENDING_RET"))

    # ── Finalize 1-gadget: previous insn was a pivot, this must be ret ─────
    updated = []
    for (pa, pi, flag) in results_1gadget:
        if flag == "PIVOT_PENDING_RET":
            if RET.match(insn):
                updated.append((pa, pi, addr))
            elif pa == addr:
                updated.append((pa, pi, flag))  # keep self
            # else: next insn wasn't ret — discard
        else:
            updated.append((pa, pi, flag))
    results_1gadget = [(pa, pi, ra) for (pa, pi, ra) in updated if ra != "PIVOT_PENDING_RET"]

    # ── 2-gadget push rdi/rdx ; pop rsp ; ret ─────────────────────────────
    if len(window) >= 1:
        prev_addr, prev_insn = window[-1]
        if POP_RSP.match(insn):
            # Check what's 1 before prev
            if PUSH_RDI.match(prev_insn) or PUSH_RDX.match(prev_insn):
                # Now we need a ret next — record pending
                results_push_pop.append((prev_addr, prev_insn, addr, None))

    push_pop_updated = []
    for (pa, pi, pop_a, ret_a) in results_push_pop:
        if ret_a is None:
            if pop_a < addr:  # next instruction after pop rsp
                if RET.match(insn):
                    push_pop_updated.append((pa, pi, pop_a, addr))
                # else discard (non-ret after pop rsp)
            else:
                push_pop_updated.append((pa, pi, pop_a, ret_a))
        else:
            push_pop_updated.append((pa, pi, pop_a, ret_a))
    results_push_pop = push_pop_updated

    # ── pop rdi ; ret ─────────────────────────────────────────────────────
    if len(window) >= 1:
        prev_addr2, prev_insn2 = window[-1]
        if POP_RDI.match(prev_insn2) and RET.match(insn):
            results_pop_rdi.append((prev_addr2, addr))

    # ── JOP chain: mov rax,[rdx+X] ; ... ; jmp rax ────────────────────────
    if MOV_RAX_MEM_RDX.match(insn):
        jop_candidate = (addr, insn)
    elif jop_candidate and JMP_RAX.match(insn):
        # Check if consecutive (at most 2 instructions apart)
        if addr - jop_candidate[0] <= 16:
            results_jop.append((jop_candidate[0], jop_candidate[1], addr))
        jop_candidate = None
    elif jop_candidate and addr - jop_candidate[0] > 24:
        jop_candidate = None

    window.append((addr, insn))

proc.wait()

print(f"[*] Processed {line_count:,} lines from objdump")
print()

# ── Report: 1-gadget pivots ──────────────────────────────────────────────────
print("=" * 70)
print("  1-GADGET STACK PIVOTS (instruction-boundary-aligned, followed by ret)")
print("=" * 70)
if results_1gadget:
    for (pa, pi, ra) in results_1gadget:
        offset = pa - 0xffffffff81000000
        print(f"  PIVOT  @ {pa:#018x}  (offset {offset:#010x})  :  {pi}")
        print(f"  RET    @ {ra:#018x}")
        print()
else:
    print("  [!] NONE FOUND — no clean 1-gadget pivot for rdi/rdx in this build")
    print()

# ── Report: 2-gadget push;pop;ret ───────────────────────────────────────────
print("=" * 70)
print("  2-STEP PIVOTS (push rdi/rdx ; pop rsp ; ret)")
print("=" * 70)
completed_pushpop = [(pa, pi, pop_a, ra) for (pa, pi, pop_a, ra) in results_push_pop if ra is not None]
if completed_pushpop:
    for (pa, pi, pop_a, ra) in completed_pushpop[:10]:
        offset = pa - 0xffffffff81000000
        print(f"  PUSH   @ {pa:#018x}  (offset {offset:#010x})  :  {pi}")
        print(f"  POP    @ {pop_a:#018x}")
        print(f"  RET    @ {ra:#018x}")
        print()
else:
    print("  [!] NONE FOUND")
    print()

# ── Report: pop rdi ; ret ────────────────────────────────────────────────────
print("=" * 70)
print("  POP_RDI_RET gadgets (pop rdi ; ret) — first 5")
print("=" * 70)
if results_pop_rdi:
    for (pa, ra) in results_pop_rdi[:5]:
        offset = pa - 0xffffffff81000000
        print(f"  POP RDI @ {pa:#018x}  (offset {offset:#010x})")
        print(f"  RET     @ {ra:#018x}")
        print()
else:
    print("  [!] NONE FOUND")
    print()

# ── Report: JOP chain candidates ────────────────────────────────────────────
print("=" * 70)
print("  JOP CHAIN candidates (mov rax,[rdx+X] ; ... ; jmp rax) — first 5")
print("=" * 70)
if results_jop:
    for (pa, pi, ja) in results_jop[:5]:
        offset = pa - 0xffffffff81000000
        print(f"  PIVOT1 @ {pa:#018x}  (offset {offset:#010x})  :  {pi}")
        print(f"  JMP    @ {ja:#018x}")
        print()
else:
    print("  [!] NONE FOUND")
    print()
