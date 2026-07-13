# ROP Provenance Audit

## Executive Summary

This data-flow audit reconstructs the precise provenance of the first ROP gadget written to the `virt + 0x120` payload slot.

**Critical Finding regarding `0xffffffff810001cd`:**
The value `0xffffffff810001cd` **does not exist** in the provenance graph or the execution trace. It is the result of an AI mathematical hallucination from a previous report (`RUNTIME_VALIDATION_REPORT.md`). The previous report incorrectly calculated the QEMU crash location `__startup_64+0x12d` as `0xffffffff810001cd`. In reality, `__startup_64` is located at `0xffffffff81000090` in the custom `vmlinux`, meaning `__startup_64+0x12d` is exactly **`0xffffffff810001bd`**. 

The true value produced by the exploit and written to the payload page is **`0xffffffff810001bd`**.

---

## Data-Flow Provenance Graph

The following trace works backward from the payload page write in `exploit.cpp` to the ultimate origin of `0xffffffff810001bd`.

### Step 1: Payload Page Memory Write
* **Location:** `exploit.cpp:1048`
* **Classification:** Observed directly in source
* **Code:**
  ```cpp
  for (size_t i = 0; i < kernel_rop.size(); i++)
      page.SetU64(rop_start_offset + 8 + (i * 8), kernel_rop[i]);
  ```
* **Analysis:** At `i = 0`, `kernel_rop[0]` is written to `rop_start_offset + 8`. Since `rop_start_offset` is `0x118`, the first ROP gadget is written exactly to `virt + 0x120`.

### Step 2: ROP Chain Generation
* **Location:** `exploit.cpp:1010` and `exploit.cpp:957`
* **Classification:** Observed directly in source
* **Code:**
  ```cpp
  std::vector<uint64_t> kernel_rop = rop_build_privesc(target);
  // ... inside rop_build_privesc ...
  RopChain rop(target, kernel_base);
  rop.AddRopAction(RopActionId::COMMIT_INIT_TASK_CREDS);
  ```
* **Analysis:** The `kernel_rop` vector is populated by `RopChain::AddRopAction` using the `COMMIT_INIT_TASK_CREDS` action ID.

### Step 3: Action Translation & Arithmetic
* **Location:** `libxdk/payloads/RopChain.cpp:22`
* **Classification:** Observed directly in source
* **Code:**
  ```cpp
  void RopChain::AddRopAction(RopActionId id, std::vector<uint64_t> arguments) {
      std::vector<RopItem> rop_items = target_.GetRopActionItems(id);
      for (auto item : rop_items) {
          // ...
          } else if (item.type == RopItemType::SYMBOL) {
              action.values.push_back(item.value + kaslr_base_);
          }
  ```
* **Analysis:** `target_.GetRopActionItems` returns a sequence of `RopItem` structs. The first item has `item.type == RopItemType::SYMBOL` (1) and `item.value == 0x1bd`. The code performs the arithmetic `0x1bd + 0xffffffff81000000` (static `kaslr_base_` fallback), resulting in `0xffffffff810001bd`.

### Step 4: Target Map Lookup
* **Location:** `libxdk/target/Target.cpp:32`
* **Classification:** Observed directly in source
* **Code:**
  ```cpp
  std::vector<RopItem> Target::GetRopActionItems(RopActionId id) {
      std::string name = RopActionNames.at(id); // Resolves to "commit_creds"
      return rop_actions[name];
  }
  ```
* **Analysis:** The `rop_actions` map stores the raw `RopItem` objects, parsed during target initialization.

### Step 5: Database Binary Parsing
* **Location:** `libxdk/target/KxdbParser.cpp:287`
* **Classification:** Observed directly in source
* **Code:**
  ```cpp
  void KxdbParser::ParseRopActions(Target& target) {
      // ...
      auto type_and_value = ReadUInt();
      auto type = (RopItemType)(type_and_value & 0x03);
      auto value = type_and_value >> 2;
      rop_items.push_back(RopItem(type, value));
  ```
* **Analysis:** The `type` and `value` are bitwise-extracted directly from an integer read from the KXDB binary stream. 

### Step 6: The Ultimate Origin (`target_db.kxdb`)
* **Location:** `target_db.kxdb` (Loaded via `INCBIN` at `exploit.cpp:33`)
* **Classification:** Derived from source / Binary Hexdump
* **Analysis:** A standalone C++ debug script (`dump_rop.cpp`) linked against `libxdk` confirms that for target `"kernelctf", "lts-6.12.67"`, the first ROP item for `commit_creds` is encoded in the binary as:
  * `type = 1` (`RopItemType::SYMBOL`)
  * `value = 0x1bd`
  
  This `0x1bd` is an arbitrary raw offset hardcoded into the `target_db.kxdb` file by the external Python generator script for the official Google kernelCTF image. It is intended to point to a `pop rdi; ret` gadget. 
  
  However, on our locally compiled `vmlinux`, `0xffffffff810001bd` lands exactly on the instruction `sldt (%rax)` inside `__startup_64`.

---

## Conclusion

The value `0xffffffff810001cd` never existed. 

The true executed value is **`0xffffffff810001bd`**. Its absolute origin is the pre-compiled `target_db.kxdb` database, which supplies the raw offset `0x1bd` for the first stage of the `COMMIT_INIT_TASK_CREDS` ROP action. This offset is blindly added to `kernel_base` by `libxdk` and pushed onto the payload page, directly resulting in the `#PF supervisor write access` fault observed in QEMU.
