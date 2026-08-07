# EXP-020 Results

## Analysis of `rb_erase_cached` with a single node

In this experiment, we tracked the execution of `rb_erase_cached` (which simply wraps `rb_erase`) when called on a freed `eventpoll` struct (at `0xfffffcb374c0`) which was sprayed via `msg_msg` allocations such that its `rbr` (RB root) field at offset 104 is attacker-controlled.

However, the node being erased is `epi->rbn`, which is a single node (representing the added pipe fd in the epoll).

During `rb_erase(node, &root->rb_root)`:
1. `node->rb_right` is read. It's NULL (0x0).
2. `node->rb_left` is read. It's NULL (0x0).
3. The code enters `if (!tmp)` (since `rb_left` is NULL).
4. `pc = node->__rb_parent_color;` is read. Since this is the only node, it's the root of the tree. The root has no parent, but it has the black color bit set. `pc = 0x1` (NULL parent, black color).
5. `parent = __rb_parent(pc);` computes to NULL (0x0).
6. `__rb_change_child(node, child, parent, root)` is called with `child = NULL` and `parent = NULL`.
7. Inside `__rb_change_child`, it checks `if (parent)`. Since `parent` is NULL, it falls through to the `else` branch:
   `WRITE_ONCE(root->rb_node, new);`
   Here, `root` is `&ep->rbr.rb_root` (offset 104). `new` is `child` (which is NULL).
8. Therefore, the kernel writes NULL (0x0) to `root->rb_node` (offset 104 of the reclaimed slab object).
9. It then returns from `rb_erase`.

## Conclusion
For a single epitem, `rb_erase` simply writes NULL to offset 104 (`ep->rbr.rb_root.rb_node`) of the UAF object. It **does not** read or dereference the attacker-controlled value that we placed there during the spray. It just blindly overwrites whatever was there with NULL because it thinks it is erasing the root node of the tree and replacing it with its child (which is also NULL).

There is no info leak or control-flow hijack at this stage when only a single node is present.

Next, we should investigate EXP-021: what happens when there are TWO nodes.
