#!/usr/bin/env python3
"""Read-only MIPS address xref finder for ELF32 guest images."""
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

REG = [
    "$zero", "$at", "$v0", "$v1", "$a0", "$a1", "$a2", "$a3",
    "$t0", "$t1", "$t2", "$t3", "$t4", "$t5", "$t6", "$t7",
    "$s0", "$s1", "$s2", "$s3", "$s4", "$s5", "$s6", "$s7",
    "$t8", "$t9", "$k0", "$k1", "$gp", "$sp", "$fp", "$ra",
]


def s16(v: int) -> int:
    return v - 0x10000 if v & 0x8000 else v


def asm(word: int) -> str:
    op = (word >> 26) & 0x3F
    rs, rt, imm = (word >> 21) & 0x1F, (word >> 16) & 0x1F, word & 0xFFFF
    if op == 0x0F:
        return f"lui {REG[rt]},0x{imm:04x}"
    if op == 0x09:
        return f"addiu {REG[rt]},{REG[rs]},{s16(imm)}"
    if op == 0x0D:
        return f"ori {REG[rt]},{REG[rs]},0x{imm:04x}"
    if op == 0x23:
        return f"lw {REG[rt]},{s16(imm)}({REG[rs]})"
    if op == 0x2B:
        return f"sw {REG[rt]},{s16(imm)}({REG[rs]})"
    return f".word 0x{word:08x}"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path)
    ap.add_argument("output", type=Path)
    ap.add_argument("--target", action="append", default=[], type=lambda x: int(x, 0))
    ap.add_argument("--range-start", type=lambda x: int(x, 0), default=None)
    ap.add_argument("--range-end", type=lambda x: int(x, 0), default=None)
    args = ap.parse_args()
    data = args.input.read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        raise ValueError("input is not ELF32 little-endian")
    phoff, phentsize, phnum = struct.unpack_from("<IHH", data, 28)
    loads = []
    for i in range(phnum):
        p_type, p_offset, p_vaddr, _p_paddr, p_filesz, _p_memsz, p_flags, _p_align = struct.unpack_from(
            "<IIIIIIII", data, phoff + i * phentsize
        )
        if p_type == 1:
            loads.append((p_offset, p_vaddr, p_filesz, p_flags))
    targets = set(args.target)
    if args.range_start is not None or args.range_end is not None:
        if args.range_start is None or args.range_end is None or args.range_end < args.range_start:
            raise ValueError("range-start and range-end must be supplied in ascending order")
        targets.update(range(args.range_start, args.range_end + 1, 4))
    if not targets:
        raise ValueError("at least one --target or a range is required")

    def file_to_vaddr(off: int) -> int | None:
        for po, pv, psz, _flags in loads:
            if po <= off < po + psz:
                return (pv + off - po) & 0xFFFFFFFF
        return None

    pointer_xrefs = []
    for po, pv, psz, _flags in loads:
        end = min(len(data), po + psz)
        for off in range(po, end - 4 + 1, 4):
            value = struct.unpack_from("<I", data, off)[0]
            if value in targets:
                pointer_xrefs.append({
                    "file_offset": off,
                    "vaddr": f"0x{file_to_vaddr(off):08x}",
                    "target": f"0x{value:08x}",
                })

    insns = []
    for po, pv, psz, flags in loads:
        if not flags & 1:
            continue
        end = min(len(data), po + psz)
        for off in range(po, end - 4 + 1, 4):
            insns.append((off, (pv + off - po) & 0xFFFFFFFF, struct.unpack_from("<I", data, off)[0]))
    insns.sort()
    literal_xrefs = []
    for idx, (off, pc, word) in enumerate(insns):
        if (word >> 26) & 0x3F != 0x0F:
            continue
        rt, hi = (word >> 16) & 0x1F, word & 0xFFFF
        for j in range(idx + 1, min(idx + 9, len(insns))):
            _off2, pc2, word2 = insns[j]
            if pc2 != pc + 4 * (j - idx):
                break
            op2 = (word2 >> 26) & 0x3F
            rs2, rt2, lo = (word2 >> 21) & 0x1F, (word2 >> 16) & 0x1F, word2 & 0xFFFF
            if rs2 != rt or rt2 != rt or op2 not in (0x09, 0x0D):
                continue
            value = ((hi << 16) + s16(lo)) & 0xFFFFFFFF if op2 == 0x09 else ((hi << 16) | lo) & 0xFFFFFFFF
            if value in targets:
                literal_xrefs.append({
                    "pc": f"0x{pc:08x}",
                    "distance_instructions": j - idx,
                    "target": f"0x{value:08x}",
                    "mode": "lui+addiu-window" if op2 == 0x09 else "lui+ori-window",
                    "instructions": [
                        {"pc": f"0x{pc:08x}", "word": f"0x{word:08x}", "asm": asm(word)},
                        {"pc": f"0x{pc2:08x}", "word": f"0x{word2:08x}", "asm": asm(word2)},
                    ],
                })

    result = {
        "input": str(args.input),
        "targets": [f"0x{x:08x}" for x in sorted(targets)],
        "pointer_xrefs": pointer_xrefs,
        "literal_xrefs": literal_xrefs,
        "method": "aligned literal pointers plus a bounded static lui/addiu/ori window; no execution or symbolic flow",
        "executed": False,
        "relocated": False,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
