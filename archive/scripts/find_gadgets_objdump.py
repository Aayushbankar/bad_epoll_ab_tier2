import subprocess
import sys
import re

def parse_elf_text_bounds(vmlinux_path):
    try:
        output = subprocess.check_output(['readelf', '-S', vmlinux_path], text=True)
        for line in output.splitlines():
            if '.text' in line:
                parts = line.split()
                idx = parts.index('.text')
                addr = int(parts[idx+2], 16)
                size = int(parts[idx+4], 16)
                return addr, addr + size
    except Exception:
        pass
    
    try:
        output = subprocess.check_output(['objdump', '-h', vmlinux_path], text=True)
        for line in output.splitlines():
            if '.text' in line:
                parts = line.split()
                size = int(parts[2], 16)
                addr = int(parts[3], 16)
                return addr, addr + size
    except Exception:
        pass
    
    return 0xffffffff81000000, 0xffffffff82500000

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 find_gadgets_objdump.py <path_to_vmlinux>")
        sys.exit(1)
        
    vmlinux_path = sys.argv[1]
    
    print("[*] Locating .text section bounds...")
    start_addr, end_addr = parse_elf_text_bounds(vmlinux_path)
    print(f"[*] .text section: 0x{start_addr:x} - 0x{end_addr:x}")
    
    print("[*] Running objdump -M intel and scanning for gadgets...")
    
    # Run with -M intel
    cmd = ['objdump', '-d', '-M', 'intel', '--section=.text', vmlinux_path]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    
    window = []
    inst_re = re.compile(r'^\s*([0-9a-fA-F]+):\s+([0-9a-f \t]+)\s+(\w+)\s*(.*)$')
    kernel_base = 0xffffffff81000000
    
    found_pivots = 0
    found_jops = 0
    
    while True:
        line = proc.stdout.readline()
        if not line:
            break
        
        m = inst_re.match(line)
        if not m:
            continue
            
        addr_str, bytes_str, mnemonic, operands = m.groups()
        addr = int(addr_str, 16)
        
        if not (start_addr <= addr < end_addr):
            continue
            
        window.append((addr, mnemonic, operands))
        if len(window) > 8:
            window.pop(0)
            
        # 1. Check for Simple Stack Pivots ending in ret
        if mnemonic in ('ret', 'retq'):
            for i in range(len(window)):
                sub_window = window[i:]
                instructions = [f"{inst[1]} {inst[2]}" for inst in sub_window]
                full_str = " ; ".join(instructions)
                
                is_pivot = False
                pivot_type = ""
                
                # Check rdi, rsi, rbp, rdx, rax, rcx pivots
                for reg in ['rdi', 'rsi', 'rdx', 'rax', 'rcx', 'rbp']:
                    if re.search(rf'xchg\s+.*rsp.*{reg}|xchg\s+.*{reg}.*rsp', full_str):
                        is_pivot = True
                        pivot_type = f"xchg rsp, {reg}"
                        break
                    elif re.search(rf'mov\s+.*rsp.*{reg}', full_str) and not re.search(rf'mov\s+.*{reg}.*rsp', full_str):
                        is_pivot = True
                        pivot_type = f"mov rsp, {reg}"
                        break
                    elif re.search(rf'push\s+{reg}', full_str) and re.search(rf'pop\s+rsp', full_str):
                        is_pivot = True
                        pivot_type = f"push {reg}; ...; pop rsp"
                        break
                
                if is_pivot:
                    gadget_addr = sub_window[0][0]
                    offset = gadget_addr - kernel_base
                    print(f"[+] Direct Pivot: {pivot_type} at vaddr=0x{gadget_addr:x} (offset=0x{offset:x})")
                    print(f"    Gadget: {full_str}\n")
                    found_pivots += 1
                    break
        
        # 2. Check for JOP/ROP chain elements (PIVOT1 - PIVOT4 semantic equivalents)
        # PIVOT1 ends in call rax or call [rax] or jmp rax
        if mnemonic in ('call', 'jmp') and operands in ('rax', 'rdx', 'rcx', 'rsi', 'rdi', 'rbx'):
            call_reg = operands
            # Let's see if the window contains: mov call_reg, [rdx + offset] and mov rdi, rdx
            full_str = " ; ".join([f"{inst[1]} {inst[2]}" for inst in window])
            
            # PIVOT1 check: must load call_reg from rdx offset, move rdx to rdi, and call/jmp
            if (f"mov rdi, rdx" in full_str or f"mov edi, edx" in full_str) and \
               re.search(rf'mov\s+{call_reg},\s*(?:QWORD PTR )?\[rdx\s*\+\s*(0x[0-9a-fA-F]+|\d+)\]', full_str):
                
                offset_match = re.search(rf'mov\s+{call_reg},\s*(?:QWORD PTR )?\[rdx\s*\+\s*(0x[0-9a-fA-F]+|\d+)\]', full_str)
                offset_val = offset_match.group(1)
                gadget_addr = window[0][0]
                offset = gadget_addr - kernel_base
                print(f"[+] JOP PIVOT1 Candidate at vaddr=0x{gadget_addr:x} (offset=0x{offset:x})")
                print(f"    Target call register: {call_reg}, rdx offset: {offset_val}")
                print(f"    Gadget: {full_str}\n")
                found_jops += 1
                
        # PIVOT4 check: push [rcx] or push rcx; ...; pop rsp; ret
        if mnemonic in ('ret', 'retq'):
            full_str = " ; ".join([f"{inst[1]} {inst[2]}" for inst in window])
            if re.search(r'push\s+(?:QWORD PTR )?\[rcx\]', full_str) and re.search(r'pop\s+rsp', full_str):
                gadget_addr = window[0][0]
                offset = gadget_addr - kernel_base
                print(f"[+] JOP PIVOT4 Candidate at vaddr=0x{gadget_addr:x} (offset=0x{offset:x})")
                print(f"    Gadget: {full_str}\n")
                found_jops += 1

    proc.wait()
    print(f"[*] Scan finished. Found {found_pivots} direct pivots and {found_jops} JOP candidates.")

if __name__ == '__main__':
    main()
