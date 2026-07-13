with open("/tmp/rsp_rdi.txt") as f:
    text = f.read()

lines = text.split('\n')
for i in range(len(lines)):
    line = lines[i].strip()
    if not line: continue
    if "mov    %rsp,%rdi" in line or "mov    %rsp,%rdx" in line or "mov    %rdi,%rsp" in line or "mov    %rdx,%rsp" in line:
        if i + 1 < len(lines):
            next_line = lines[i+1].strip()
            if next_line.endswith("ret") or next_line.endswith("retq"):
                print(line)
                print(next_line)
                print("---")
