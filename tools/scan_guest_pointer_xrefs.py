#!/usr/bin/env python3
"""Find raw and simple lui/addiu references to guest data pointers."""
from pathlib import Path
import struct
import sys

path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("artifacts/input/SCUS_973.99.elf")
data = path.read_bytes()
text_off, text_va, text_size = 0x1000, 0x00100000, 0x19bba4
targets = {
    0x002c0a28: "sio2man",
    0x002c0a38: "ptr_after_sio2man",
    0x002c0a40: "dbcman",
    0x002c0a50: "ptr_after_dbcman",
    0x002c0a58: "sio2d",
    0x002c0a68: "ptr_after_sio2d",
    0x002c0a70: "mc2_d",
    0x002c0a80: "ptr_after_mc2_d",
    0x002c0a88: "ds2u_d",
    0x002c0a98: "ptr_after_ds2u_d",
    0x002c0aa0: "libsd",
    0x002c0ab0: "ptr_after_libsd",
    0x002c0ab8: "989nomid",
    0x002c0ac8: "smpd_iop",
    0x002c0ad8: "ptr_after_smpd_iop",
}

print(f"ELF={path}")
for addr, name in targets.items():
    needle = struct.pack("<I", addr)
    hits = []
    pos = 0
    while True:
        pos = data.find(needle, pos)
        if pos < 0:
            break
        hits.append(pos)
        pos += 1
    print(f"{name}: guest=0x{addr:08x} raw_refs={len(hits)} offsets=" + ",".join(f"0x{x:x}" for x in hits[:20]))

print("simple_lui_addiu_refs:")
words = [struct.unpack_from("<I", data, text_off + i)[0] for i in range(0, text_size, 4)]
for i, word in enumerate(words):
    op = (word >> 26) & 0x3f
    rt = (word >> 16) & 31
    if op != 0x0f or (word & 0xffff) != 0x002c:
        continue
    for j in range(i + 1, min(i + 12, len(words))):
        w2 = words[j]
        op2 = (w2 >> 26) & 0x3f
        rs2 = (w2 >> 21) & 31
        rt2 = (w2 >> 16) & 31
        imm = w2 & 0xffff
        if rt2 != rt:
            continue
        if op2 == 0x09:
            value = (0x002c0000 + (imm if imm < 0x8000 else imm - 0x10000)) & 0xffffffff
        elif op2 == 0x0d:
            value = 0x002c0000 | imm
        else:
            continue
        if value in targets:
            print(f"{targets[value]}: lui@0x{text_va+i*4:08x} pair@0x{text_va+j*4:08x} value=0x{value:08x}")
        break
