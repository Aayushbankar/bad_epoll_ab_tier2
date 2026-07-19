# CVE-2026-46242 — Runtime Pointer Relationship Validation
# This script will:
# 1. Connect to QEMU
# 2. Boot kernel until ep_remove clears f_ep
# 3. Examine the exact pointer relationships at runtime
# 4. Prove that epi->fllink.pprev points into struct eventpoll

set pagination off
set confirm off
set architecture aarch64
file tier2/android/artifacts/vmlinux
target remote :1234

# Breakpoint at the instruction BEFORE f_ep is cleared
# 0xffffffc008407f70: str xzr, [x22, #0xd0]  ; file->f_ep = NULL
break *0xffffffc008407f70

# Define commands to execute when breakpoint 1 is hit
commands 1
  echo \n========================================\n
  echo === HIT: ep_remove f_ep=NULL STORE ===\n
  echo ========================================\n
  
  # At this point:
  # x19 = struct eventpoll *ep (outer_epoll)
  # x20 = struct epitem *epi (outer's watch item)
  # x22 = struct file *file (inner_file = epi->ffd.file)
  # The instruction is about to store NULL to file->f_ep
  
  echo \n--- REGISTER STATE ---\n
  echo ep (outer_epoll) = x19:\n
  print/x $x19
  echo epi (outer watch item) = x20:\n
  print/x $x20
  echo file (inner_file) = x22:\n
  print/x $x22
  
  echo \n--- CURRENT file->f_ep (before clearing) ---\n
  # file->f_ep is at [x22 + 0xd0]
  echo file->f_ep = 
  x/gx $x22 + 0xd0
  
  echo \n--- file->private_data (inner eventpoll) ---\n
  # file->private_data is at [x22 + 0xc8]
  echo file->private_data = 
  x/gx $x22 + 0xc8
  
  echo \n--- epi->fllink.pprev ---\n
  # epi->fllink.pprev is at [x20 + 0x58]
  echo epi->fllink.pprev = 
  x/gx $x20 + 0x58
  
  echo \n--- epi->ep (eventpoll this item belongs to) ---\n
  # epi->ep is at [x20 + 0x48]
  echo epi->ep = 
  x/gx $x20 + 0x48
  
  echo \n--- CRITICAL RELATIONSHIP CHECK ---\n
  echo inner_epoll = file->private_data\n
  echo inner_epoll->refs.first is at (inner_epoll + 0xa0)\n
  echo epi->fllink.pprev should == &inner_epoll->refs.first\n
  echo \nComputing expected pprev target:\n
  # Read inner_epoll address from file->private_data
  set $inner_ep = *(unsigned long *)($x22 + 0xc8)
  echo inner_epoll address = 
  print/x $inner_ep
  echo &inner_epoll->refs.first = inner_epoll + 0xa0 = 
  print/x $inner_ep + 0xa0
  echo actual epi->fllink.pprev = 
  set $actual_pprev = *(unsigned long *)($x20 + 0x58)
  print/x $actual_pprev
  
  echo \n=== MATCH CHECK ===\n
  if $actual_pprev == $inner_ep + 0xa0
    echo *** CONFIRMED: epi->fllink.pprev points to inner_epoll->refs.first ***\n
    echo *** This proves the UAF write target is inside struct eventpoll ***\n
  else
    echo pprev does NOT point to inner_epoll->refs.first\n
    echo Checking if file->f_ep == &inner_epoll->refs:\n
    set $f_ep = *(unsigned long *)($x22 + 0xd0)
    print/x $f_ep
    echo pprev target points to address: 
    print/x $actual_pprev
  end
  
  echo \n--- file->f_op (to verify this is an epoll file) ---\n
  echo file->f_op = 
  x/gx $x22 + 0x28
  
  echo \n========================================\n
  echo === EXAMINATION COMPLETE ===\n
  echo ========================================\n
  
  # Continue to let kernel proceed (may hit again for next iteration)
  continue
end

echo \n=== BREAKPOINTS CONFIGURED. BOOTING... ===\n
continue
