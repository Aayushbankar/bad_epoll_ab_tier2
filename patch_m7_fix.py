import re

with open("tier2/scripts/test_m7_spray.c", "rb") as f:
    text = f.read().decode('utf-8')

# Let's completely replace the closure block.
old_block = """    close(inner);
    for (int i = 0; i < 1000; i++) close(batch_a[i]);
    for (int i = 0; i < 1000; i++) close(batch_b[i]);
    for (int i = 0; i < 4000; i++) close(bg_fds[i]);
    
    // Wait for all to reach buddy!
    sleep(2);"""

new_block = """
    close(inner);
    sleep(1);

    long peak_slabs = read_filp_slabs_count();
    printf("[*] Peak filp slabs = %ld\\n", peak_slabs);
    
    for (int i = 0; i < 1000; i++) close(batch_a[i]);
    for (int i = 0; i < 1000; i++) close(batch_b[i]);
    for (int i = 0; i < 4000; i++) close(bg_fds[i]);
    
    // Wait for all to reach buddy!
    sleep(2);

    long post_drain_slabs = read_filp_slabs_count();
    printf("[*] Post-drain filp slabs = %ld (delta = %ld)\\n", post_drain_slabs, peak_slabs - post_drain_slabs);
"""

if old_block in text:
    text = text.replace(old_block, new_block)
else:
    print("Could not find old_block! regex fallback")
    # try regex
    pattern = re.compile(r"close\(inner\);.*?sleep\(2\);", re.DOTALL)
    text = pattern.sub(new_block.strip(), text)

with open("tier2/scripts/test_m7_spray.c", "wb") as f:
    f.write(text.encode('utf-8'))
