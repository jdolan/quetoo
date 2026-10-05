---
name: win-crash-debug
description: Symbolicate and analyze a Windows crash dump (.dmp, or a .zip that holds one) attached to a Quetoo GitHub issue, with the matching release PDBs. Use when an issue includes a Windows crash dump.
---

# Analyze a Windows crash dump

Use this skill when a GitHub issue includes a Windows crash dump (`.dmp`, or a `.zip` that holds
one). The tool is `src/tools/symbolicate_dmp.py`.

## 1. Download the dump

```sh
gh issue view <NUMBER> -R jdolan/quetoo --comments   # find the attachment URL
WORK=$(mktemp -d)
curl -L -o "$WORK/crash.zip" "<URL>"
```

The script accepts the `.zip` directly, or the extracted `.dmp`.

## 2. Read the dump without symbols

```sh
python3 src/tools/symbolicate_dmp.py "$WORK/crash.zip"
```

This prints the loaded modules (base address, size and name), the PDB GUID of `quetoo.exe`, the
exception, the registers and the `quetoo.exe` frames of the stack. Note:

- **The faulting module.** Compare the crash address with each module's base address and size.
  The script symbolicates only `quetoo.exe`. A crash in `cgame.dll`, `game.dll` or an Objectively
  DLL needs that module's PDB, and the manual lookup in step 4.
- **The release.** The script prints the GUID of `quetoo.exe` only, for example
  `{678461D0-8922-0672-4C4C-44205044422E}`. Find the release whose `quetoo.pdb` matches it (step 3).
  The DLL PDBs in that same ZIP belong to the same build.

Without `--pdb`, the script tries to download matching symbols. It has two limits:

- It searches only the 20 most recent releases. Quetoo releases almost every day, so a dump that is
  a few weeks old can be older than every release it searches.
- It extracts only `quetoo.pdb`. For any other module, download the PDBs yourself (step 3).
- It does not extract `quetoo.exe`. Without `quetoo.exe` beside the PDB, the script uses section
  addresses hardcoded from one old build, and the symbols can be wrong. Prefer step 3, which keeps
  `quetoo.exe` beside `quetoo.pdb`.

## 3. Download the matching PDBs

The PDBs are inside the Windows release ZIP:

```sh
gh release list -R jdolan/quetoo --limit 50
gh release download <TAG> -R jdolan/quetoo --pattern 'quetoo-x86_64-pc-windows.zip' --dir "$WORK/sym"
unzip -q "$WORK/sym/"*.zip -d "$WORK/sym"
```

| PDB | Path in the ZIP |
|---|---|
| `quetoo.pdb`, `quetoo-dedicated.pdb`, `quemap.pdb` | `bin/` |
| `Objectively.pdb`, `ObjectivelyGPU.pdb`, `ObjectivelyMVC.pdb` | `bin/` |
| `cgame.pdb`, `game.pdb` | `lib/default/`, `lib/ctf/`, `lib/lithium/` (one pair for each game module) |

The tag `latest` is the snapshot from the most recent push to `main`. It is replaced on each push,
so a dump from an older snapshot has no PDBs left anywhere.

**Verify the GUID before you trust any symbol.** A PDB from another build produces stacks that look
plausible and are wrong. Split the GUID `{AAAAAAAA-BBBB-CCCC-DDDDDDDDDDDDDDDD}` into its fields:

```python
import struct
guid = struct.pack("<IHH", 0xAAAAAAAA, 0xBBBB, 0xCCCC) + bytes.fromhex("DDDDDDDDDDDDDDDD")
assert guid in open("bin/quetoo.pdb", "rb").read(2_000_000)
```

## 4. Symbolicate

```sh
python3 src/tools/symbolicate_dmp.py "$WORK/crash.zip" --pdb "$WORK/sym/bin/quetoo.pdb"
```

The output gives the exception type, the crash address as `Function+offset`, the faulting address,
the registers, and the `quetoo.exe` frames of the stack, symbolicated.

For an address in another module, subtract that module's base address (from the module list) to get
an RVA, and find the public symbol at or below that RVA in the module's PDB:

```sh
/opt/homebrew/opt/llvm/bin/llvm-pdbutil dump --publics "$WORK/sym/lib/default/cgame.pdb"
```

The script's stack scan keeps only return addresses inside `quetoo.exe`, so it does not list the
frames of other modules. The crash address and `Rip` are always printed. For deeper frames in a DLL,
extend `find_return_addresses` in the script to accept that module's address range.

## 5. Interpret

- **`ACCESS_VIOLATION` (`0xC0000005`).** `ExceptionInformation[0]` is 0 for a read and 1 for a write.
  `ExceptionInformation[1]` is the address. A near-null address or `0xffffffffffffffff` is a null or
  garbage pointer. Find which register held it, and trace that value back through the stack.
- **`STACK_OVERFLOW` (`0xC00000FD`).** Deep recursion or a very large stack frame. The stack shows
  the repeated frames.
- **An illegal instruction or a divide by zero.** Read the code at the crash address.

To disassemble the code around the crash, use the `quetoo.exe` (or the DLL) from the same ZIP:

```sh
/opt/homebrew/opt/llvm/bin/llvm-objdump -d --start-address=0x<VA> --stop-address=0x<VA_END> bin/quetoo.exe
```

## Pitfalls

1. **The exception stream is minidump stream type 6.** Type 7 is `SystemInfoStream`.
2. **Use the `CONTEXT` from the exception stream** (`ThreadContext`, at offset 160 of that stream).
   The crashed thread's `CONTEXT` in `ThreadListStream` can be corrupt.
3. **The x64 `CONTEXT` offsets are not sequential:** `Rax` is at `0x78`, `Rsp` at `0x98`, `Rdi` at
   `0xB0` and `Rip` at `0xF8`.
4. **Section addresses change with every build.** Read them from the PE header of the binary in the
   matching release. Do not reuse them from another build.

## Tools

- `gh`, authenticated, to download release assets.
- `python3`, for `src/tools/symbolicate_dmp.py`.
- `llvm-objdump` and `llvm-pdbutil` from `brew install llvm`, to disassemble and to dump PDB symbols.
