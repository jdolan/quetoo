# Skill: Windows Crash Dump Analysis (win-crash-debug)

Use this skill whenever a GitHub issue contains a Windows crash dump (`.dmp` or zipped `.dmp`).

---

## Workflow

### Step 1 — Download the dump

Download the `.zip` attachment from the GitHub issue.  If it is not a direct URL you can use `gh`:

```bash
gh issue view <NUMBER> --repo jdolan/quetoo
# copy the attachment URL, then:
curl -L -o /tmp/crash_<NUMBER>.zip "<URL>"
```

Extract it:

```bash
mkdir -p /tmp/quetoo_<NUMBER>
unzip /tmp/crash_<NUMBER>.zip -d /tmp/quetoo_<NUMBER>/
```

---

### Step 2 — Identify the symbols version from the dump

**Never skip this step.** Symbols from the wrong release produce garbage output.

Run the script to extract the PDB GUID from the dump:

```bash
python3 src/tools/symbolicate_dmp.py /tmp/quetoo_<NUMBER>/crash.dmp
```

The script prints the GUID under **LOADED MODULES**.  The format is:
```
PDB GUID:  {678461D0-8922-0672-4C4C-44205044422E}
```

---

### Step 3 — Download the matching symbols

The script auto-downloads when it can. If you need to do it manually:

```bash
# List recent releases
gh release list --repo jdolan/quetoo

# Download symbols for the matching version (e.g. v1.0.19)
gh release download v1.0.19 --repo jdolan/quetoo \
    --pattern "*windows*symbols*" --dir /tmp/symbols_v1019/

unzip /tmp/symbols_v1019/*.zip -d /tmp/symbols_v1019/
```

Verify the GUID matches by scanning the PDB for the raw GUID bytes:

```python
import struct
d1, d2, d3 = 0x678461D0, 0x8922, 0x0672
guid_bytes = struct.pack("<IHH", d1, d2, d3) + bytes.fromhex("4C4C44205044422E")
assert guid_bytes in open("/tmp/symbols_v1019/bin/quetoo.pdb","rb").read(2_000_000)
```

---

### Step 4 — Full analysis with symbols

```bash
python3 src/tools/symbolicate_dmp.py /tmp/quetoo_<NUMBER>/crash.dmp \
    --pdb /tmp/symbols_v1019/bin/quetoo.pdb
```

This produces:
- **Exception type** (ACCESS_VIOLATION, STACK_OVERFLOW, etc.)
- **Crash address + symbol** (e.g. `Cl_LoadMedia+1412`)
- **Fault address** (for AV: the bad pointer being dereferenced)
- **All registers** (RAX–RIP)
- **Stack trace** with symbolicated return addresses

---

### Step 5 — Interpret the crash

#### ACCESS_VIOLATION (0xC0000005)
- `ExceptionInformation[0] = 0` → read fault, `= 1` → write fault
- `ExceptionInformation[1]` = the faulting address
- `0xffffffffffffffff` or near-null → NULL/garbage pointer dereference
- Check: which variable/pointer was in the register at crash? (RDI, RBX, RAX…)
- Use the stack trace to determine the call chain leading to the crash

#### STACK_OVERFLOW (0xC00000FD)
- Deep recursion or very large stack frame
- Stack trace will show the recursive call

#### Illegal instruction / divide-by-zero
- Check the exact crash address in `src/` and look for the surrounding code logic

---

### Step 6 — Cross-reference source code

Once you have `Function+offset`, find it in source:

```bash
# Disassemble the region (if you have cstool)
brew install capstone   # macOS
cstool x64 <hex_bytes_at_crash_rva> <crash_va>

# Or use llvm-objdump on quetoo.exe
/opt/homebrew/opt/llvm/bin/llvm-objdump -d \
    --start-address=0x<rva_hex> --stop-address=0x<end_rva_hex> quetoo.exe
```

---

## Key Constants

| Constant                     | Value        | Meaning                          |
|------------------------------|-------------|----------------------------------|
| ExceptionStream type         | **6**       | NOT 7 (that's SystemInfoStream)  |
| CONTEXT.RIP offset           | 0xF8        |                                  |
| CONTEXT.RSP offset           | 0x98        |                                  |
| CONTEXT.RAX offset           | 0x78        |                                  |
| CONTEXT.RDI offset           | 0xB0        |                                  |
| quetoo.exe .text section     | VA 0x1000   | section 1                        |
| quetoo.exe .rdata section    | VA 0xF8000  | section 2                        |
| quetoo.exe .data section     | VA 0x1D8000 | section 3                        |

---

## Tools Required

| Tool         | Install                       | Purpose                        |
|--------------|-------------------------------|--------------------------------|
| `gh`         | system or `brew install gh`   | Download release assets        |
| `llvm-pdbutil` | `brew install llvm`         | Dump PDB public symbols        |
| `python3`    | system                        | Run `src/tools/symbolicate_dmp.py` |

---

## Script Reference

```
src/tools/symbolicate_dmp.py <dump_or_zip> [--pdb <quetoo.pdb>]
```

- Accepts `.dmp` or `.zip` directly
- Without `--pdb`: attempts to auto-download matching symbols from GitHub releases
- With `--pdb`: uses provided PDB (faster, use when you already have the right symbols)
- Outputs: stream directory, module list, exception info, registers, crash summary, stack trace

---

## Common Pitfalls

1. **Never parse stream type 7 as ExceptionStream.** Type 7 is SystemInfoStream. ExceptionStream is **type 6**.
2. **Always verify PDB GUID before trusting symbolication.** The wrong PDB produces plausible-looking but wrong results.
3. **x64 CONTEXT offsets are not sequential.** Use the table above; don't guess.
4. **The crash thread's CONTEXT in ThreadListStream may be corrupt.** Always use the CONTEXT from ExceptionStream (ThreadContext at offset 160 of the exception stream).
