with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

# Add getpid before close(inner)
old_code = """    // Trigger race
    close(inner);"""
new_code = """    // Trigger race (GDB-assisted survivor)
    printf("[*] Calling getpid() to arm GDB hook...\\n");
    fflush(stdout);
    getpid();
    
    close(inner);"""
text = text.replace(old_code, new_code)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
