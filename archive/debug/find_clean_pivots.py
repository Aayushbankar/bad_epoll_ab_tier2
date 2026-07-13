with open("/tmp/ropgadget.txt") as f:
    for line in f:
        line = line.strip()
        if line.endswith("pop rsp ; ret"):
            print(line)
        if line.endswith("mov rsp, rdx ; ret"):
            print(line)
        if line.endswith("mov rsp, rdi ; ret"):
            print(line)
        if line.endswith("xchg rsp, rdi ; ret"):
            print(line)
        if line.endswith("xchg rsp, rdx ; ret"):
            print(line)
        if line.endswith("pop rdi ; ret"):
            print(line)
