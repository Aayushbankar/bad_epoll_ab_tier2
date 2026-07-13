import sys, re

text = sys.stdin.read()
lines = text.split('\n')
window = []
for line in lines:
    if not line: continue
    m = re.search(r':\s*[0-9a-f ]+\s+([^\n]+)', line)
    if not m: continue
    insn = m.group(1).strip()
    
    window.append((line, insn))
    if len(window) > 4:
        window.pop(0)
        
    if 'ret' in window[-1][1] or 'jmp' in window[-1][1] or 'call' in window[-1][1]:
        # push rdx ; pop rsp
        if any('pop    %rsp' in w[1] or 'pop    rsp' in w[1] for w in window):
            if any('push   %rdx' in w[1] or 'push   rdx' in w[1] for w in window):
                for w in window:
                    print(w[0])
                print("---")
        # mov rsp, rdx
        if any('mov    %rdx,%rsp' in w[1] or 'mov    rdx,rsp' in w[1] or 'mov    %rdx, %rsp' in w[1] for w in window):
            for w in window:
                print(w[0])
            print("---")
        # xchg rdx, rsp
        if any('xchg   %rdx,%rsp' in w[1] or 'xchg   %rsp,%rdx' in w[1] or 'xchg   rdx,rsp' in w[1] for w in window):
            for w in window:
                print(w[0])
            print("---")
            
        # PIVOT from rdi instead of rdx
        # push rdi ; pop rsp
        if any('pop    %rsp' in w[1] or 'pop    rsp' in w[1] for w in window):
            if any('push   %rdi' in w[1] or 'push   rdi' in w[1] for w in window):
                for w in window:
                    print(w[0])
                print("---")
        # mov rsp, rdi
        if any('mov    %rdi,%rsp' in w[1] or 'mov    rdi,rsp' in w[1] for w in window):
            for w in window:
                print(w[0])
            print("---")
        # xchg rdi, rsp
        if any('xchg   %rdi,%rsp' in w[1] or 'xchg   %rsp,%rdi' in w[1] or 'xchg   rdi,rsp' in w[1] for w in window):
            for w in window:
                print(w[0])
            print("---")
