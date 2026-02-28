#!/usr/bin/env python3
"""Audit Battlespire BSA archives for record/compression patterns.

Writes:
- batspire/research_phase4/bsa_archive_summary.csv
- batspire/research_phase4/bsa_flag_inventory.csv
"""
from __future__ import annotations
import csv
import struct
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT_DIR = ROOT / "batspire" / "research_phase4"
OUT_DIR.mkdir(parents=True, exist_ok=True)


def parse_bsa(path: Path):
    b = path.read_bytes()
    if len(b) < 4:
        return None
    record_count, record_type = struct.unpack_from("<HH", b, 0)
    if record_type == 0x100:
        entry_size = 18
    elif record_type == 0x200:
        entry_size = 8
    else:
        entry_size = 6
    footer_bytes = record_count * entry_size
    if len(b) < 4 + footer_bytes:
        return None

    footer_start = len(b) - footer_bytes
    running = 4 if record_type in (0x100, 0x200) else 2
    flags = Counter()
    ext_counts = Counter()

    for i in range(record_count):
        p = footer_start + i * entry_size
        if record_type == 0x100:
            name_raw = b[p:p + 12].split(b"\x00", 1)[0].decode("latin1", errors="replace").strip()
            flag = struct.unpack_from("<H", b, p + 12)[0]
            packed = struct.unpack_from("<I", b, p + 14)[0]
            name = name_raw
        elif record_type == 0x200:
            flag = struct.unpack_from("<H", b, p + 0)[0]
            rec_id = struct.unpack_from("<H", b, p + 2)[0]
            packed = struct.unpack_from("<I", b, p + 4)[0]
            name = f"REC_{rec_id}"
        else:
            flag = 0
            rec_id = struct.unpack_from("<H", b, p + 0)[0]
            packed = struct.unpack_from("<I", b, p + 2)[0]
            name = f"REC_{rec_id}"

        flags[flag] += 1
        ext = name.rsplit(".", 1)[-1].upper() if "." in name else ""
        ext_counts[ext] += 1

        running += packed

    return {
        "record_count": record_count,
        "record_type": record_type,
        "entry_size": entry_size,
        "footer_start": footer_start,
        "file_size": len(b),
        "flags": flags,
        "ext_counts": ext_counts,
    }


def main() -> int:
    bsa_paths = sorted((ROOT / "batspire").glob("*.BSA"))
    if not bsa_paths:
        print("No BSA files found under batspire/*.BSA")
        return 0

    archive_rows = []
    flag_rows = []

    for p in bsa_paths:
        data = parse_bsa(p)
        if not data:
            archive_rows.append([p.name, "invalid", "", "", "", "", ""])
            continue

        top_ext = ";".join(f"{k}:{v}" for k, v in data["ext_counts"].most_common(8) if k)
        archive_rows.append([
            p.name,
            data["record_type"],
            data["record_count"],
            data["entry_size"],
            data["footer_start"],
            data["file_size"],
            top_ext,
        ])

        for flag, cnt in sorted(data["flags"].items()):
            flag_rows.append([p.name, data["record_type"], f"0x{flag:04X}", cnt, int((flag & 1) != 0)])

    with (OUT_DIR / "bsa_archive_summary.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["archive", "record_type", "record_count", "entry_size", "footer_start", "file_size", "top_extensions"])
        w.writerows(archive_rows)

    with (OUT_DIR / "bsa_flag_inventory.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["archive", "record_type", "compression_flag", "entry_count", "compressed_low_bit"])
        w.writerows(flag_rows)

    print(f"Wrote {OUT_DIR / 'bsa_archive_summary.csv'}")
    print(f"Wrote {OUT_DIR / 'bsa_flag_inventory.csv'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
