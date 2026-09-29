#!/usr/bin/env python3
"""Pack a guest ELF into a Shkodya .exe (header + flat image).

    python3 tools/pack_exe.py --elf build64/demo.elf --out build64/test.exe \
                              --symbol shk_main --base 0x1000000

Produces:  shk_exe_header_t (16 B) || objcopy -O binary image

The entry offset is read from the ELF symbol table rather than hardcoded, so
moving the entry point in the guest source cannot silently produce a .exe that
jumps into the middle of a function.
"""
import argparse
import os
import struct
import subprocess
import sys

MAGIC = b"SHKE"
HEADER_SIZE = 16


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("error: %s failed:\n%s" % (" ".join(cmd), r.stderr.strip()))
    return r.stdout


def symbol_address(elf, name):
    out = run(["readelf", "-sW", elf])
    for line in out.splitlines():
        parts = line.split()
        # columns: Num: Value Size Type Bind Vis Ndx Name
        if len(parts) >= 8 and parts[-1] == name:
            return int(parts[1], 16)
    sys.exit("error: symbol %r not found in %s" % (name, elf))


def section_size(elf, name):
    """Size of a section, from `readelf -SW`.

    Columns: [Nr] Name Type Address Off Size ES Flg Lk Inf Al
    so the size sits four tokens after the name (name + type + addr + off).
    """
    out = run(["readelf", "-SW", elf])
    for line in out.splitlines():
        parts = line.split()
        for i, tok in enumerate(parts):
            if tok == name and i + 4 < len(parts):
                try:
                    return int(parts[i + 4], 16)
                except ValueError:
                    break
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--elf", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--symbol", default="shk_main")
    ap.add_argument("--base", default="0x1000000")
    ap.add_argument("--magic", default="SHKE", choices=("SHKE", "MZ"))
    args = ap.parse_args()

    if not os.path.isfile(args.elf):
        sys.exit("error: %s not found" % args.elf)
    base = int(args.base, 16)

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    flat = args.out + ".flat"
    # -O binary drops section headers and produces the raw loadable image
    run(["objcopy", "-O", "binary", "-j", ".text", "-j", ".data", args.elf, flat])
    blob = open(flat, "rb").read()

    entry_addr = symbol_address(args.elf, args.symbol)
    entry_offset = entry_addr - base
    text_size = section_size(args.elf, ".text") or len(blob)
    if text_size > len(blob):
        sys.exit("error: .text (%d) larger than the flat image (%d)" % (text_size, len(blob)))
    data_size = len(blob) - text_size

    # --- validation the loader will repeat at runtime -----------------------
    # entry_offset is measured from the start of the *image*, not the start of
    # the file: the loader copies everything after the 16-byte header down to
    # --base and jumps to base + entry_offset. Comparing it against
    # HEADER_SIZE..HEADER_SIZE+text_size would reject any guest whose entry
    # point sits in the first 16 bytes of its code, which is exactly what
    # happens to shk_main once -O2 makes it the first function in .text.
    if not (0 <= entry_offset < text_size):
        sys.exit("error: entry 0x%x is not inside the code section "
                 "(image spans 0x0..0x%x)"
                 % (entry_offset, text_size))

    magic = MAGIC if args.magic == "SHKE" else b"MZ\x00\x00"
    header = struct.pack("<4sIII", magic, text_size, data_size, entry_offset)
    assert len(header) == HEADER_SIZE

    with open(args.out, "wb") as fh:
        fh.write(header)
        fh.write(blob)

    os.unlink(flat)
    print("packed %s -> %s" % (os.path.basename(args.elf), args.out))
    print("  magic        : %r" % magic)
    print("  text_size    : %d" % text_size)
    print("  data_size    : %d" % data_size)
    print("  entry_offset : 0x%x  (%s @ 0x%x, base 0x%x)"
          % (entry_offset, args.symbol, entry_addr, base))
    print("  total file   : %d bytes (%d header + %d image)"
          % (os.path.getsize(args.out), HEADER_SIZE, len(blob)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
