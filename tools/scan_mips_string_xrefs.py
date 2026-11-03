#!/usr/bin/env python3
"""Find simple MIPS LA sequences that form selected guest string addresses."""
from pathlib import Path
import struct
import sys

ELF = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("artifacts/input/SCUS_973.99.elf")
DATA = ELF.read_bytes()
TEXT_OFF = 0x1000
TEXT_VA = 0x00100000
TEXT_SIZE = 0x19bba4
TARGETS = {
    0x002b29c8: "IOP_MOD",
    0x002c0a28: "sio2man.irx",
    0x002c0a40: "dbcman.irx",
    0x002c0a58: "sio2d.irx",
    0x002c0a70: "mc2_d.irx",
    0x002c0a88: "ds2u_d.irx",
    0x002c0aa0: "libsd.irx",
    0x002c0ab8: "989nomid.irx",
    0x002c0ac8: "smpd_iop.irx",
    0x002c5300: "rom0:ROMVER",
    0x002c6d40: "rom0:UDNL",
}


def fields(word):
    return (word >> 26, (word >> 21) & 31, (word >> 16) & 31, word & 0xffff)


def fmt(word):
    op, rs, rt, imm = fields(word)
    return f"0x{word:08x} op=0x{op:02x} rs={rs} rt={rt} imm=0x{imm:04x}"


hits = []
end = min(len(DATA), TEXT_OFF + TEXT_SIZE)
words = [struct.unpack_from("<I", DATA, off)[0] for off in range(TEXT_OFF, end - 3, 4)]
for i, word in enumerate(words):
    op, rs, rt, imm = fields(word)
    if op != 0x0f:  # LUI
        continue
    upper = imm << 16
    if upper != 0x002c0000:
        continue
    for j in range(i + 1, min(i + 9, len(words))):
        op2, rs2, rt2, imm2 = fields(words[j])
        if rt2 != rt or (op2 not in (0x09, 0x0d)):
            continue
        # ADDIU sign-extends; ORI zero-extends.
        low = imm2 if op2 == 0x0d else (imm2 if imm2 < 0x8000 else imm2 - 0x10000)
        value = (upper + low) & 0xffffffff if op2 == 0x09 else upper | (imm2 & 0xffff)
        if value in TARGETS:
            va0 = TEXT_VA + (TEXT_OFF + i * 4 - TEXT_OFF)
            va1 = TEXT_VA + (TEXT_OFF + j * 4 - TEXT_OFF)
            hits.append((va0, va1, value, TARGETS[value], rt, word, words[j]))
        break

print(f"ELF={ELF}")
print(f"text_file=0x{TEXT_OFF:x} text_va=0x{TEXT_VA:x} text_size=0x{TEXT_SIZE:x}")
print(f"hits={len(hits)}")
for va0, va1, value, name, rt, w0, w1 in hits:
    print(f"{name}: guest=0x{value:08x} lui@0x{va0:08x} pair@0x{va1:08x} rt={rt} {fmt(w0)} ; {fmt(w1)}")

for addr, name in TARGETS.items():
    if not any(h[2] == addr for h in hits):
        print(f"{name}: guest=0x{addr:08x} no_simple_lui_pair")
