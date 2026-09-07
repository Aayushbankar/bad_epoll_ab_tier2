# HYP-013 Oops Analysis (Stage-2 Reclaim Miss)

## Disassembly of `__ep_remove` (+0xb0 to +0xf0)
```text
   0xffffffc008407528 <+176>:	movk	w9, #0xdead, lsl #16
   0xffffffc00840752c <+180>:	cmp	x8, x9
   0xffffffc008407530 <+184>:	b.eq	0xffffffc008407700 <__ep_remove+648>
   0xffffffc008407534 <+188>:	add	x22, x21, #0x30
   0xffffffc008407538 <+192>:	mov	x0, x22
   0xffffffc00840753c <+196>:	bl	0xffffffc009040bf0 <_raw_spin_lock>
   0xffffffc008407540 <+200>:	ldrb	w8, [x20, #60]
   0xffffffc008407544 <+204>:	cbz	w8, 0xffffffc008407558 <__ep_remove+224>
   0xffffffc008407548 <+208>:	tbnz	w23, #0, 0xffffffc008407558 <__ep_remove+224>
   0xffffffc00840754c <+212>:	mov	x0, x22
   0xffffffc008407550 <+216>:	bl	0xffffffc009040e88 <_raw_spin_unlock>
   0xffffffc008407554 <+220>:	b	0xffffffc00840778c <__ep_remove+788>
   0xffffffc008407558 <+224>:	ldr	x23, [x21, #208]
   0xffffffc00840755c <+228>:	add	x9, x20, #0x50
   0xffffffc008407560 <+232>:	ldr	x8, [x23]
   0xffffffc008407564 <+236>:	cmp	x8, x9
   0xffffffc008407568 <+240>:	b.eq	0xffffffc008407588 <__ep_remove+272>
```

## Instruction and Source Mapping
- **Fault Instruction**: `0xffffffc008407560 <+232>` (`+0xe8`): `ldr x8, [x23]`
- **Producer of x23**: `0xffffffc008407558 <+224>` (`+0xe0`): `ldr x23, [x21, #208]`
- **Source Map**: `fs/eventpoll.c:845`
  - `head = file->f_ep;` (Producer: loads `file->f_ep` at offset 208 from `file` pointer `x21` into `x23`)
  - `if (head->first == &epi->fllink ...)` (Fault: dereferences `head` pointer `x23` to read `head->first` into `x8`)

## Register State at Fault Time
Based on the `__ep_remove` setup block:
- **`x19` (`ep`)**: `0xffffff8004566780` — The outer `struct eventpoll`. It resides at a distance of `0xC0` (192 bytes) from the freed inner_ep (`0xffffff80045666c0`), indicating `x19` is the adjacent `kmalloc-192` object allocated right next to the inner_ep.
- **`x20` (`epi`)**: `0xffffff8004567300` — The survivor `struct epitem` still attached to the outer `ep`.
- **`x21` (`file`)**: `0xffffff8004565000` — The freed `struct file` corresponding to the inner eventpoll.
- **`x23` (`head`)**: `0` — The `f_ep` list head pointer loaded from `file->f_ep`. Because the memory was zero-filled by the buddy allocator/kzalloc, it evaluates to NULL.

## Causal Chain of the Panic
When the natural race succeeds, the inner `struct file` is freed while its `struct epitem` (the survivor) remains linked in the outer eventpoll's (`ep_uaf_waiter`) RB-tree. To reclaim the freed `struct file` memory, the harness performs a victim sandwich drain followed by a dma-buf spray. If this Stage-2 reclaim misses (meaning the sprayed fake file objects do not land on the exact memory address previously held by the inner `struct file`), the freed memory remains zero-filled either by the buddy allocator (`init_on_free=1`) or by an unrelated allocation. 

Upon detecting the AAR Oracle Miss, the harness attempts to "Clean up for retry" by executing `close(ep_uaf_waiter)`. This triggers the following sequence:
1. `close(ep_uaf_waiter)` invokes `ep_free()` on the outer eventpoll.
2. `ep_free()` iterates through its RB-tree to destroy all remaining epitems, including the survivor `epi`.
3. It calls `__ep_remove(ep, epi)`.
4. Inside `__ep_remove`, `epi->ffd.file` resolves to the freed, zeroed memory (in `x21`).
5. The code attempts to traverse the file's epoll hooks list: `head = file->f_ep;` (`ldr x23, [x21, #208]`), loading `0` into `x23`.
6. The condition `head->first == &epi->fllink` is evaluated. The dereference of `head->first` (`ldr x8, [x23]`) causes a terminal read fault on `VA 0x0`.

If the harness exits entirely instead of looping, process exit triggers the same `close()` path on the outer eventpoll, culminating in the same panic.
