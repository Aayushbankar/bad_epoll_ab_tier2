import sys
import struct
import re

def parse_elf_segments(vmlinux_path):
    with open(vmlinux_path, 'rb') as f:
        elf_header = f.read(64)
        if elf_header[:4] != b'\x7fELF':
            raise ValueError("Not an ELF file")
        
        phoff = struct.unpack('<Q', elf_header[32:40])[0]
        phnum = struct.unpack('<H', elf_header[56:58])[0]
        phentsize = struct.unpack('<H', elf_header[54:56])[0]
        
        segments = []
        f.seek(phoff)
        for _ in range(phnum):
            ph = f.read(phentsize)
            if len(ph) < 56:
                break
            p_type = struct.unpack('<I', ph[0:4])[0]
            p_offset = struct.unpack('<Q', ph[8:16])[0]
            p_vaddr = struct.unpack('<Q', ph[16:24])[0]
            p_memsz = struct.unpack('<Q', ph[40:48])[0]
            if p_type == 1: # PT_LOAD
                segments.append((p_offset, p_vaddr, p_memsz))
        return segments

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 find_gadgets.py <path_to_vmlinux>")
        sys.exit(1)
        
    vmlinux_path = sys.argv[1]
    
    try:
        segments = parse_elf_segments(vmlinux_path)
    except Exception as e:
        print(f"Error parsing ELF: {e}")
        sys.exit(1)
        
    with open(vmlinux_path, 'rb') as f:
        data = f.read()
        
    kernel_base = 0xffffffff81000000

    def print_match(name, offset, info=""):
        vaddr = None
        for p_off, p_vaddr, p_memsz in segments:
            if p_off <= offset < p_off + p_memsz:
                vaddr = p_vaddr + (offset - p_off)
                break
        if vaddr:
            print(f"  [+] {name} {info}: offset=0x{vaddr - kernel_base:x} (vaddr=0x{vaddr:x})")
        else:
            print(f"  [-] {name} at file offset 0x{offset:x} (not mapped)")

    # 1. Search for Simple Stack Pivots
    print("=== Searching for Simple Stack Pivots ===")
    simple_pivots = {
        "push rdi; pop rsp; ret": b'\x57\x5c\xc3',
        "push rsi; pop rsp; ret": b'\x56\x5c\xc3',
        "push rdx; pop rsp; ret": b'\x52\x5c\xc3',
        "push rcx; pop rsp; ret": b'\x51\x5c\xc3',
        "push rax; pop rsp; ret": b'\x50\x5c\xc3',
        "mov rsp, rdi; ret": b'\x48\x89\xfc\xc3',
        "mov rsp, rsi; ret": b'\x48\x89\xf4\xc3',
        "mov rsp, rdx; ret": b'\x48\x89\xd4\xc3',
        "mov rsp, rcx; ret": b'\x48\x89\xcc\xc3',
        "mov rsp, rax; ret": b'\x48\x89\xc4\xc3',
        "xchg rsp, rdi; ret": b'\x48\x97\xc3',
    }
    for name, pattern in simple_pivots.items():
        idx = data.find(pattern)
        while idx != -1:
            print_match(name, idx)
            idx = data.find(pattern, idx + 1)

    # 2. Search for LTS Gadgets using regex (wildcarding offsets and registers)
    print("\n=== Searching for JOP/ROP chain elements (wildcarded) ===")
    
    # PIVOT1: mov rax, [rdx + offset]; mov [rsp + 0x20], rdx; mov rdi, rdx; mov rax, [rax]; call rax
    # \x48\x8b\x42. -> mov rax, [rdx+offset]
    # \x48\x89\x54\x24\x20 -> mov [rsp+0x20], rdx
    # \x48\x89\xd7 -> mov rdi, rdx
    # \x48\x8b\x00 -> mov rax, [rax]
    # \xff\xd0 -> call rax
    p1_regex = re.compile(b'\x48\x8b\x42.\x48\x89\x54\x24\x20\x48\x89\xd7\x48\x8b\x00\xff\xd0')
    for m in p1_regex.finditer(data):
        offset = m.start()
        val = m.group(0)[3] # the offset byte
        print_match("PIVOT1", offset, f"(offset=0x{val:x})")

    # PIVOT2: mov rax, [rdi]; mov rbx, rdi; mov r13, [rdi+offset]; mov r12, [rdi+offset]; mov rax, [rax+offset]; call rax
    # \x48\x8b\x07 -> mov rax, [rdi]
    # \x48\x89\xfb -> mov rbx, rdi
    # \x4c\x8b\x6f. -> mov r13, [rdi+offset]
    # \x4c\x8b\x67. -> mov r12, [rdi+offset]
    # (\x48\x8b\x80.... | \x48\x8b\x40.) -> mov rax, [rax+offset]
    # \xff\xd0 -> call rax
    p2_regex = re.compile(b'\x48\x8b\x07\x48\x89\xfb\x4c\x8b\x6f.\x4c\x8b\x67.(?:\x48\x8b\x80....|\x48\x8b\x40.)\xff\xd0')
    for m in p2_regex.finditer(data):
        offset = m.start()
        print_match("PIVOT2", offset)

    # PIVOT3: mov rax, [rdi + offset]; mov rcx, [rdi + offset]; mov rdx, [rdi + offset]; mov rax, [rax + offset]; mov rdi, rcx; jmp rax
    # \x48\x8b\x47. -> mov rax, [rdi+offset]
    # \x48\x8b\x8f.... -> mov rcx, [rdi+offset]
    # \x48\x8b\x57. -> mov rdx, [rdi+offset]
    # \x48\x8b\x40. -> mov rax, [rax+offset]
    # \x48\x89\xcf -> mov rdi, rcx
    # \xff\xe0 -> jmp rax
    p3_regex = re.compile(b'\x48\x8b\x47.\x48\x8b\x8f....\x48\x8b\x57.\x48\x8b\x40.\x48\x89\xcf\xff\xe0')
    for m in p3_regex.finditer(data):
        offset = m.start()
        print_match("PIVOT3", offset)

    # PIVOT4: push [rcx]; rcr/etc; pop rsp; ret
    # \xff\x31 -> push [rcx]
    # \xc0\x53.41 -> rcr byte [rbx+offset], 0x41
    # \x5c -> pop rsp
    # \xc3 -> ret
    p4_regex = re.compile(b'\xff\x31\xc0\x53.\x41\x5c\xc3')
    for m in p4_regex.finditer(data):
        offset = m.start()
        print_match("PIVOT4", offset)

if __name__ == '__main__':
    main()
