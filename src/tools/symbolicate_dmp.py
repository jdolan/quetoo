#!/usr/bin/env python3
"""
symbolicate_dmp.py - Quetoo Windows crash dump analyzer

Usage:
    python3 scripts/symbolicate_dmp.py <path_to_dump.dmp> [--pdb <path_to_quetoo.pdb>]

If --pdb is not given, the script downloads the matching symbols from GitHub
releases automatically (requires `gh` CLI to be authenticated).

Dependencies:
    - Python 3.8+
    - gh CLI (for auto-download of symbols)
    - llvm-pdbutil  (brew install llvm  on macOS)
    - cstool        (brew install capstone  on macOS, or pip install capstone)
"""

import argparse
import os
import re
import struct
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

# ---------------------------------------------------------------------------
# Minidump stream type constants
# ---------------------------------------------------------------------------
STREAM_THREAD_LIST   = 3
STREAM_MODULE_LIST   = 4
STREAM_MEMORY_LIST   = 5
STREAM_EXCEPTION     = 6    # ExceptionStream — NOT 7!
STREAM_SYSTEM_INFO   = 7
STREAM_MEMORY64_LIST = 9
STREAM_MISC_INFO     = 15

# ---------------------------------------------------------------------------
# x64 CONTEXT register offsets (byte offset from start of CONTEXT struct)
# ---------------------------------------------------------------------------
CTX_RAX = 0x78
CTX_RCX = 0x80
CTX_RDX = 0x88
CTX_RBX = 0x90
CTX_RSP = 0x98
CTX_RBP = 0xA0
CTX_RSI = 0xA8
CTX_RDI = 0xB0
CTX_R8  = 0xB8
CTX_R9  = 0xC0
CTX_R10 = 0xC8
CTX_R11 = 0xD0
CTX_R12 = 0xD8
CTX_R13 = 0xE0
CTX_R14 = 0xE8
CTX_R15 = 0xF0
CTX_RIP = 0xF8

EXCEPTION_ACCESS_VIOLATION = 0xC0000005
EXCEPTION_STACK_OVERFLOW   = 0xC00000FD
EXCEPTION_ILLEGAL_INSTR    = 0xC000001D
EXCEPTION_INT_DIVIDE_ZERO  = 0xC0000094
EXCEPTION_BREAKPOINT       = 0x80000003

EXCEPTION_NAMES = {
    EXCEPTION_ACCESS_VIOLATION: "ACCESS_VIOLATION",
    EXCEPTION_STACK_OVERFLOW:   "STACK_OVERFLOW",
    EXCEPTION_ILLEGAL_INSTR:    "ILLEGAL_INSTRUCTION",
    EXCEPTION_INT_DIVIDE_ZERO:  "INTEGER_DIVIDE_BY_ZERO",
    EXCEPTION_BREAKPOINT:       "BREAKPOINT",
}


def read_u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def read_u64(data, off):
    return struct.unpack_from("<Q", data, off)[0]


def read_str(data, off, size):
    raw = data[off:off + size]
    return raw.rstrip(b"\x00").decode("utf-8", errors="replace")


# ---------------------------------------------------------------------------
# Minidump parsing
# ---------------------------------------------------------------------------

def parse_header(data):
    """Returns (stream_count, stream_dir_rva) from MINIDUMP_HEADER."""
    sig = data[0:4]
    if sig != b"MDMP":
        raise ValueError(f"Not a minidump file (magic={sig!r})")
    # version(4), version_impl(4), stream_count(4), stream_dir_rva(4), ...
    stream_count = read_u32(data, 8)
    stream_dir_rva = read_u32(data, 12)
    return stream_count, stream_dir_rva


def parse_stream_directory(data, stream_count, stream_dir_rva):
    """Returns dict {stream_type: (data_size, rva)}."""
    streams = {}
    off = stream_dir_rva
    for _ in range(stream_count):
        stype = read_u32(data, off)
        size  = read_u32(data, off + 4)
        rva   = read_u32(data, off + 8)
        streams[stype] = (size, rva)
        off += 12
    return streams


def parse_exception_stream(data, rva):
    """Returns dict with exception info from MINIDUMP_EXCEPTION_STREAM."""
    thread_id   = read_u32(data, rva)
    exc_code    = read_u32(data, rva + 8)
    exc_flags   = read_u32(data, rva + 12)
    exc_addr    = read_u64(data, rva + 24)
    num_params  = read_u32(data, rva + 32)
    params = []
    for i in range(min(num_params, 15)):
        params.append(read_u64(data, rva + 40 + i * 8))
    ctx_size = read_u32(data, rva + 160)
    ctx_rva  = read_u32(data, rva + 164)
    return {
        "thread_id":  thread_id,
        "exc_code":   exc_code,
        "exc_flags":  exc_flags,
        "exc_addr":   exc_addr,
        "params":     params,
        "ctx_size":   ctx_size,
        "ctx_rva":    ctx_rva,
    }


def parse_context(data, ctx_rva, ctx_size):
    """Returns dict of register values from an x64 CONTEXT."""
    if ctx_size < CTX_RIP + 8:
        return {}
    base = ctx_rva
    regs = {}
    for name, off in [
        ("RAX", CTX_RAX), ("RCX", CTX_RCX), ("RDX", CTX_RDX), ("RBX", CTX_RBX),
        ("RSP", CTX_RSP), ("RBP", CTX_RBP), ("RSI", CTX_RSI), ("RDI", CTX_RDI),
        ("R8",  CTX_R8),  ("R9",  CTX_R9),  ("R10", CTX_R10), ("R11", CTX_R11),
        ("R12", CTX_R12), ("R13", CTX_R13), ("R14", CTX_R14), ("R15", CTX_R15),
        ("RIP", CTX_RIP),
    ]:
        if base + off + 8 <= len(data):
            regs[name] = read_u64(data, base + off)
    return regs


def parse_module_list(data, rva):
    """Returns list of (base_addr, size, name, pdb_guid, pdb_age, pdb_filename) tuples.

    MINIDUMP_MODULE layout (108 bytes):
      +0   BaseOfImage   (8)
      +8   SizeOfImage   (4)
      +12  CheckSum      (4)
      +16  TimeDateStamp (4)
      +20  ModuleNameRva (4)
      +24  VS_FIXEDFILEINFO VersionInfo (52)
      +76  CvRecord.DataSize (4)
      +80  CvRecord.Rva     (4)
      +84  MiscRecord.DataSize (4)
      +88  MiscRecord.Rva     (4)
      +92  Reserved0 (8)
      +100 Reserved1 (8)
    """
    count = read_u32(data, rva)
    modules = []
    off = rva + 4
    for _ in range(count):
        base     = read_u64(data, off)
        size     = read_u32(data, off + 8)
        name_rva = read_u32(data, off + 20)
        cv_size  = read_u32(data, off + 76)
        cv_rva   = read_u32(data, off + 80)

        # Read module name (MINIDUMP_STRING: u32 byte-length + UTF-16LE chars)
        name = ""
        if name_rva and name_rva + 4 < len(data):
            str_len = read_u32(data, name_rva)
            if str_len and name_rva + 4 + str_len <= len(data):
                name = data[name_rva + 4:name_rva + 4 + str_len].decode("utf-16-le", errors="replace")

        # Parse PDB GUID from CodeView record (RSDS signature)
        pdb_guid = None
        pdb_age  = None
        pdb_file = ""
        if cv_rva and cv_size >= 24 and cv_rva + cv_size <= len(data):
            sig = data[cv_rva:cv_rva + 4]
            if sig == b"RSDS":
                # GUID: Data1(4) Data2(2) Data3(2) Data4(8)
                d1 = read_u32(data, cv_rva + 4)
                d2 = struct.unpack_from("<H", data, cv_rva + 8)[0]
                d3 = struct.unpack_from("<H", data, cv_rva + 10)[0]
                d4 = data[cv_rva + 12:cv_rva + 20]
                pdb_guid = (d1, d2, d3, d4)
                pdb_age  = read_u32(data, cv_rva + 20)
                pdb_file = data[cv_rva + 24:cv_rva + cv_size].rstrip(b"\x00").decode("utf-8", errors="replace")

        modules.append((base, size, name, pdb_guid, pdb_age, pdb_file))
        off += 108  # MINIDUMP_MODULE is exactly 108 bytes
    return modules


def parse_memory_list(data, rva):
    """Returns list of (base_addr, size, file_offset) from MemoryListStream."""
    count = read_u32(data, rva)
    regions = []
    off = rva + 4
    for _ in range(count):
        base     = read_u64(data, off)
        alloc_base = read_u64(data, off + 8)
        alloc_prot = read_u32(data, off + 16)
        region_size= read_u64(data, off + 24)
        state    = read_u32(data, off + 32)
        protect  = read_u32(data, off + 36)
        mtype    = read_u32(data, off + 40)
        mem_rva  = read_u32(data, off + 44)
        mem_size = read_u32(data, off + 48)
        regions.append((base, mem_size, mem_rva))
        off += 52
    return regions


def read_mem(data, regions, va, size):
    """Read `size` bytes from virtual address `va` using the memory list."""
    for (base, region_size, file_off) in regions:
        if base <= va < base + region_size:
            offset_in = va - base
            return data[file_off + offset_in:file_off + offset_in + size]
    return None


def find_return_addresses(data, regions, rsp, exe_base, exe_size, max_scan=512):
    """Scan stack from RSP for return addresses within quetoo.exe."""
    addrs = []
    for i in range(max_scan):
        va = rsp + i * 8
        qword = read_mem(data, regions, va, 8)
        if qword and len(qword) == 8:
            val = struct.unpack("<Q", qword)[0]
            if exe_base < val < exe_base + exe_size:
                rva = val - exe_base
                addrs.append((va, val, rva))
    return addrs


def format_pdb_guid(guid):
    d1, d2, d3, d4 = guid
    return f"{{{d1:08X}-{d2:04X}-{d3:04X}-{d4[:2].hex().upper()}-{d4[2:].hex().upper()}}}"


# ---------------------------------------------------------------------------
# PDB symbolication via llvm-pdbutil
# ---------------------------------------------------------------------------

def find_llvm_pdbutil():
    candidates = [
        "/opt/homebrew/opt/llvm/bin/llvm-pdbutil",
        "/usr/local/opt/llvm/bin/llvm-pdbutil",
        "llvm-pdbutil",
    ]
    for c in candidates:
        if os.path.isfile(c) or subprocess.run(["which", c], capture_output=True).returncode == 0:
            return c
    return None


def load_pdb_symbols(pdb_path):
    """
    Returns dict {name: rva} for public symbols in the PDB.
    Uses llvm-pdbutil to dump public symbols.
    """
    llvm = find_llvm_pdbutil()
    if not llvm:
        print("WARNING: llvm-pdbutil not found; skipping symbolication.")
        print("  Install with: brew install llvm")
        return {}

    result = subprocess.run(
        [llvm, "dump", "--publics", str(pdb_path)],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"WARNING: llvm-pdbutil failed: {result.stderr[:200]}")
        return {}

    # Parse lines like:
    #   flags = function, addr = 0001:001234
    symbols = {}
    current_name = None
    for line in result.stdout.splitlines():
        line = line.strip()
        m = re.search(r'`([^`]+)`', line)
        if m:
            current_name = m.group(1)
        if current_name and "addr =" in line:
            m2 = re.search(r'addr\s*=\s*(\w+):(\w+)', line)
            if m2:
                section = int(m2.group(1))   # decimal section number
                offset  = int(m2.group(2))   # decimal offset within section
                # We need the PE section VAs to convert to RVA.
                # Store raw for now; resolve later after PE sections are known.
                symbols[current_name] = (section, offset)
                current_name = None
    return symbols


def get_pe_section_vas(pdb_path):
    """
    Infer PE section VAs from common Quetoo layout:
      .text  starts at VA 0x1000  (section 1)
      .rdata starts at VA 0xf8000 (section 2)
      .data  starts at VA 0x1d8000 (section 3)
    If the PDB lives alongside the PE, read it from there.
    """
    # Try to find quetoo.exe next to the PDB
    exe_path = pdb_path.parent / "quetoo.exe"
    if exe_path.exists():
        return parse_pe_sections(exe_path)
    # Fall back to known defaults
    return {1: 0x1000, 2: 0xF8000, 3: 0x1D8000}


def parse_pe_sections(exe_path):
    """Parse section VAs from a PE file."""
    sections = {}
    with open(exe_path, "rb") as f:
        data = f.read(0x400)  # enough for the header
    # MZ → PE offset at 0x3C
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    num_sections = struct.unpack_from("<H", data, pe_off + 6)[0]
    opt_size = struct.unpack_from("<H", data, pe_off + 20)[0]
    section_off = pe_off + 24 + opt_size
    for i in range(num_sections):
        off = section_off + i * 40
        va = struct.unpack_from("<I", data, off + 12)[0]
        sections[i + 1] = va
    return sections


def resolve_symbols(raw_symbols, section_vas):
    """Convert (section, offset) → RVA using PE section VAs."""
    resolved = {}
    for name, (section, offset) in raw_symbols.items():
        if section in section_vas:
            rva = section_vas[section] + offset
            resolved[name] = rva
    return resolved


def symbolicate_rva(rva, symbols_by_rva):
    """Return nearest symbol name + offset for a given RVA."""
    if not symbols_by_rva:
        return None
    best_name = None
    best_rva  = -1
    for name, sym_rva in symbols_by_rva.items():
        if sym_rva <= rva and sym_rva > best_rva:
            best_rva  = sym_rva
            best_name = name
    if best_name:
        return f"{best_name}+{rva - best_rva}"
    return None


# ---------------------------------------------------------------------------
# GitHub release symbol download
# ---------------------------------------------------------------------------

def find_quetoo_pdb_guid_from_dump(data, modules):
    """Find the PDB GUID for quetoo.exe from the module list."""
    for (base, size, name, pdb_guid, pdb_age, pdb_file) in modules:
        if "quetoo.exe" in name.lower() or "quetoo" in pdb_file.lower():
            return pdb_guid, pdb_age, pdb_file, base, size
    return None, None, "", 0, 0


def download_symbols(pdb_guid, pdb_age, dest_dir):
    """
    Download the matching symbols ZIP from GitHub releases using the gh CLI.
    Matches by trying tags in descending order and verifying the GUID.
    """
    print("Fetching release list from GitHub...")
    result = subprocess.run(
        ["gh", "release", "list", "--repo", "jdolan/quetoo", "--limit", "20", "--json", "tagName"],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print(f"ERROR: gh release list failed: {result.stderr}")
        return None

    import json
    releases = json.loads(result.stdout)

    for rel in releases:
        tag = rel["tagName"]
        print(f"  Trying {tag}...")
        zip_name = f"quetoo-{tag.lstrip('v')}-windows-symbols.zip"
        # Try common naming patterns
        for pattern in [
            f"*windows*symbols*",
            f"*symbols*windows*",
            "*symbols*",
        ]:
            dl = subprocess.run(
                ["gh", "release", "download", tag,
                 "--repo", "jdolan/quetoo",
                 "--pattern", pattern,
                 "--dir", str(dest_dir),
                 "--clobber"],
                capture_output=True, text=True
            )
            if dl.returncode == 0:
                # Find any downloaded zip
                zips = list(dest_dir.glob("*.zip"))
                for z in zips:
                    pdb = extract_pdb_from_zip(z, dest_dir)
                    if pdb and verify_pdb_guid(pdb, pdb_guid):
                        print(f"  ✓ Matched symbols from {tag}: {pdb}")
                        return pdb
                    elif pdb:
                        print(f"  ✗ GUID mismatch for {tag}, trying next release...")
                        pdb.unlink(missing_ok=True)
                break

    print("ERROR: Could not find matching symbols in any recent release.")
    return None


def extract_pdb_from_zip(zip_path, dest_dir):
    """Extract quetoo.pdb from a zip archive."""
    try:
        with zipfile.ZipFile(zip_path, "r") as z:
            for name in z.namelist():
                if name.lower().endswith("quetoo.pdb"):
                    z.extract(name, dest_dir)
                    return dest_dir / name
    except Exception as e:
        print(f"  WARNING: Failed to read {zip_path}: {e}")
    return None


def verify_pdb_guid(pdb_path, expected_guid):
    """Check whether pdb_path GUID matches expected_guid by scanning the file."""
    if not expected_guid:
        return True  # can't verify, assume ok
    d1, d2, d3, d4 = expected_guid
    # Build the 16-byte GUID in little-endian Windows format
    guid_bytes = struct.pack("<IHH", d1, d2, d3) + bytes(d4)
    try:
        with open(pdb_path, "rb") as f:
            content = f.read(2 * 1024 * 1024)  # first 2MB is enough
        return guid_bytes in content
    except Exception:
        return False


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Symbolicate a Quetoo Windows crash dump")
    parser.add_argument("dump", help="Path to .dmp file (or .zip containing .dmp)")
    parser.add_argument("--pdb", help="Path to quetoo.pdb (skip auto-download)")
    args = parser.parse_args()

    dump_path = Path(args.dump)
    if not dump_path.exists():
        print(f"ERROR: {dump_path} not found")
        sys.exit(1)

    tmpdir = tempfile.mkdtemp(prefix="quetoo_dmp_")
    print(f"Working directory: {tmpdir}")

    # --- Extract if zip ---
    if dump_path.suffix.lower() == ".zip":
        print(f"Extracting {dump_path}...")
        with zipfile.ZipFile(dump_path, "r") as z:
            z.extractall(tmpdir)
        dmps = list(Path(tmpdir).rglob("*.dmp"))
        if not dmps:
            print("ERROR: No .dmp file found in zip")
            sys.exit(1)
        dump_path = dmps[0]
        print(f"Found dump: {dump_path}")

    # --- Load dump ---
    print(f"Loading {dump_path} ({dump_path.stat().st_size // 1024 // 1024} MB)...")
    with open(dump_path, "rb") as f:
        data = f.read()

    # --- Parse header and streams ---
    stream_count, stream_dir_rva = parse_header(data)
    streams = parse_stream_directory(data, stream_count, stream_dir_rva)

    print(f"\n{'='*60}")
    print("STREAM DIRECTORY")
    print(f"{'='*60}")
    for stype, (sz, rva) in sorted(streams.items()):
        known = {3:"ThreadList", 4:"ModuleList", 5:"MemoryList",
                 6:"Exception", 7:"SystemInfo", 9:"Memory64List", 15:"MiscInfo"}
        print(f"  Type {stype:2d} ({known.get(stype,'?'):12s}): size={sz}, rva=0x{rva:x}")

    # --- Parse module list ---
    modules = []
    if STREAM_MODULE_LIST in streams:
        _, rva = streams[STREAM_MODULE_LIST]
        modules = parse_module_list(data, rva)

    # --- Find quetoo.exe ---
    pdb_guid, pdb_age, pdb_file, exe_base, exe_size = find_quetoo_pdb_guid_from_dump(data, modules)

    print(f"\n{'='*60}")
    print("LOADED MODULES")
    print(f"{'='*60}")
    for (base, size, name, guid, age, pdbf) in modules:
        flag = " ◄ quetoo.exe" if base == exe_base else ""
        print(f"  0x{base:016x}  {size//1024:6d}KB  {Path(name).name}{flag}")

    if exe_base:
        print(f"\n  quetoo.exe base:  0x{exe_base:016x}")
        if pdb_guid:
            print(f"  PDB GUID:         {format_pdb_guid(pdb_guid)}")
            print(f"  PDB file:         {pdb_file}")

    # --- Parse exception stream ---
    if STREAM_EXCEPTION not in streams:
        print("\nERROR: No ExceptionStream (type 6) in dump.")
        sys.exit(1)

    _, exc_rva = streams[STREAM_EXCEPTION]
    exc = parse_exception_stream(data, exc_rva)

    exc_name = EXCEPTION_NAMES.get(exc["exc_code"], f"0x{exc['exc_code']:08X}")
    exc_rva_in_exe = exc["exc_addr"] - exe_base if exe_base else 0

    print(f"\n{'='*60}")
    print("EXCEPTION")
    print(f"{'='*60}")
    print(f"  Thread ID:        0x{exc['thread_id']:x}")
    print(f"  Exception code:   0x{exc['exc_code']:08X}  ({exc_name})")
    print(f"  Exception address: 0x{exc['exc_addr']:016x}  (RVA 0x{exc_rva_in_exe:x})")
    if exc["exc_code"] == EXCEPTION_ACCESS_VIOLATION and len(exc["params"]) >= 2:
        op = "write" if exc["params"][0] else "read"
        print(f"  Fault type:       {op}")
        print(f"  Fault address:    0x{exc['params'][1]:016x}")

    # --- Parse exception context (registers) ---
    regs = {}
    if exc["ctx_rva"] and exc["ctx_size"]:
        regs = parse_context(data, exc["ctx_rva"], exc["ctx_size"])

    if regs:
        print(f"\n{'='*60}")
        print("REGISTERS (exception context)")
        print(f"{'='*60}")
        reg_order = ["RAX","RBX","RCX","RDX","RSI","RDI","RBP","RSP",
                     "R8","R9","R10","R11","R12","R13","R14","R15","RIP"]
        for i, name in enumerate(reg_order):
            if name in regs:
                suffix = ""
                if name == "RIP" and exe_base:
                    suffix = f"  (RVA 0x{regs[name] - exe_base:x})"
                print(f"  {name:4s} = 0x{regs[name]:016x}{suffix}")

    # --- Parse memory list ---
    memory_regions = []
    if STREAM_MEMORY_LIST in streams:
        _, mem_rva = streams[STREAM_MEMORY_LIST]
        memory_regions = parse_memory_list(data, mem_rva)

    # --- Stack trace ---
    rsp = regs.get("RSP")
    stack_addrs = []
    if rsp and exe_base and exe_size:
        stack_addrs = find_return_addresses(data, memory_regions, rsp, exe_base, exe_size)

    # --- Download or locate PDB ---
    pdb_path = None
    if args.pdb:
        pdb_path = Path(args.pdb)
        if not pdb_path.exists():
            print(f"ERROR: PDB not found: {pdb_path}")
            sys.exit(1)
        print(f"\nUsing provided PDB: {pdb_path}")
    elif pdb_guid:
        sym_dir = Path(tmpdir) / "symbols"
        sym_dir.mkdir(exist_ok=True)
        print(f"\nAuto-downloading matching symbols (GUID={format_pdb_guid(pdb_guid)})...")
        pdb_path = download_symbols(pdb_guid, pdb_age, sym_dir)
    else:
        print("\nWARNING: No PDB GUID found; cannot auto-download symbols.")

    # --- Symbolicate ---
    symbols_by_rva = {}
    if pdb_path:
        section_vas = get_pe_section_vas(pdb_path)
        print(f"\nPE section VAs: {', '.join(f'{k}=0x{v:x}' for k,v in sorted(section_vas.items()))}")
        raw_syms = load_pdb_symbols(pdb_path)
        symbols_by_rva = resolve_symbols(raw_syms, section_vas)
        print(f"Loaded {len(symbols_by_rva)} public symbols from PDB")

    crash_rva = exc_rva_in_exe
    crash_sym = symbolicate_rva(crash_rva, symbols_by_rva) if crash_rva else None

    print(f"\n{'='*60}")
    print("CRASH SUMMARY")
    print(f"{'='*60}")
    print(f"  Exception:    {exc_name}")
    print(f"  Crash at:     0x{exc['exc_addr']:016x}  RVA=0x{crash_rva:x}")
    if crash_sym:
        print(f"  Symbol:       {crash_sym}")
    if exc["exc_code"] == EXCEPTION_ACCESS_VIOLATION and len(exc["params"]) >= 2:
        op = "write" if exc["params"][0] else "read"
        print(f"  Fault:        {op} from 0x{exc['params'][1]:016x}")

    if stack_addrs:
        print(f"\n  Stack trace (quetoo.exe return addresses):")
        for (stack_va, ret_va, rva) in stack_addrs[:24]:
            sym = symbolicate_rva(rva, symbols_by_rva)
            sym_str = f"  ← {sym}" if sym else ""
            print(f"    [rsp+{stack_va - rsp:#06x}]  0x{ret_va:016x}  RVA=0x{rva:x}{sym_str}")

    print(f"\n{'='*60}")


if __name__ == "__main__":
    main()
