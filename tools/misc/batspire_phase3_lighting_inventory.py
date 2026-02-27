#!/usr/bin/env python3
import csv
import glob
import json
import os
import struct
from collections import Counter, defaultdict
from pathlib import Path

GROUPS = {b'GNRL', b'TEXI', b'STRU', b'SNAP', b'VIEW', b'CTRL', b'LINK', b'OBJS', b'OBJD', b'LITS', b'LITD', b'FLAS', b'FLAD'}
TARGET = {b'LITS', b'LITD', b'BRIT', b'AMBI', b'FLAS', b'FLAD', b'RAWD'}
TARGET_NAMES = sorted(n.decode('latin1') for n in TARGET)


def u32(b, o):
    return struct.unpack_from('<I', b, o)[0]


def parse_bs6(path: str):
    b = open(path, 'rb').read()
    counts = Counter()
    lengths = defaultdict(Counter)
    scalar_stats = defaultdict(lambda: {'min': None, 'max': None, 'samples': 0})

    def note_scalars(name: str, payload: bytes):
        if len(payload) < 4 or len(payload) % 4 != 0:
            return
        vals = struct.unpack('<' + 'i' * (len(payload) // 4), payload)
        s = scalar_stats[name]
        s['samples'] += len(vals)
        lo = min(vals)
        hi = max(vals)
        s['min'] = lo if s['min'] is None else min(s['min'], lo)
        s['max'] = hi if s['max'] is None else max(s['max'], hi)

    def walk(start, end):
        i = start
        while i + 8 <= end:
            name = b[i:i + 4]
            ln = u32(b, i + 4)
            j = i + 8 + ln
            if j > end:
                return
            payload = b[i + 8:j]

            if name in TARGET:
                key = name.decode('latin1')
                counts[key] += 1
                lengths[key][ln] += 1
                note_scalars(key, payload)

            if name in GROUPS:
                walk(i + 8, j)
            i = j

    walk(0, len(b))
    return counts, lengths, scalar_stats


def main():
    out_dir = Path('batspire/research_phase3')
    out_dir.mkdir(parents=True, exist_ok=True)

    files = sorted(glob.glob('batspire/BS6_extracted/*.BS6'))
    per_file = []
    global_counts = Counter()
    global_lengths = defaultdict(Counter)
    global_scalars = defaultdict(lambda: {'min': None, 'max': None, 'samples': 0})

    for p in files:
        counts, lengths, scalars = parse_bs6(p)
        row = {'file': os.path.basename(p)}
        for key in TARGET_NAMES:
            row[f'{key.lower()}_count'] = counts.get(key, 0)
        per_file.append(row)

        global_counts.update(counts)
        for k, v in lengths.items():
            global_lengths[k].update(v)
        for k, s in scalars.items():
            g = global_scalars[k]
            if s['samples'] == 0:
                continue
            g['samples'] += s['samples']
            g['min'] = s['min'] if g['min'] is None else min(g['min'], s['min'])
            g['max'] = s['max'] if g['max'] is None else max(g['max'], s['max'])

    per_file_csv = out_dir / 'lighting_chunk_inventory.csv'
    with per_file_csv.open('w', newline='') as f:
        cols = ['file'] + [f"{k.lower()}_count" for k in TARGET_NAMES]
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(per_file)

    length_csv = out_dir / 'lighting_payload_lengths.csv'
    with length_csv.open('w', newline='') as f:
        w = csv.writer(f)
        w.writerow(['chunk', 'payload_len', 'count'])
        for chunk in sorted(global_lengths.keys()):
            for ln, count in sorted(global_lengths[chunk].items()):
                w.writerow([chunk, ln, count])

    manifest = {
        'bs6_files_scanned': len(files),
        'global_chunk_counts': {k: v for k, v in sorted(global_counts.items())},
        'scalar_probe': {
            k: {'samples': v['samples'], 'min': v['min'], 'max': v['max']}
            for k, v in sorted(global_scalars.items())
            if v['samples'] > 0
        },
    }
    (out_dir / 'phase3_lighting_manifest.json').write_text(json.dumps(manifest, indent=2, sort_keys=True) + '\n')

    md = []
    md.append('# Phase 3 Lighting/Material Inventory')
    md.append('')
    md.append(f'- BS6 files scanned: **{len(files)}**')
    for k, v in sorted(global_counts.items()):
        md.append(f'- `{k}` chunk count: **{v}**')
    md.append('')
    md.append('## Scalar probe ranges (signed int32, exploratory)')
    md.append('| chunk | samples | min | max |')
    md.append('|---|---:|---:|---:|')
    for k, v in sorted(global_scalars.items()):
        if v['samples'] <= 0:
            continue
        md.append(f"| `{k}` | {v['samples']} | {v['min']} | {v['max']} |")
    md.append('')
    md.append('## Notes')
    md.append('1. This is a structure-first inventory pass for Phase 3 and does not yet decode semantic fields.')
    md.append('2. Use `lighting_payload_lengths.csv` to prioritize reverse-engineering the most common payload shapes.')
    md.append('3. Next step: wire decoded light terms into renderer debug overlays for parity iteration.')
    (out_dir / 'PHASE3_LIGHTING_ANALYSIS.md').write_text('\n'.join(md) + '\n')

    print('wrote', per_file_csv)
    print('wrote', length_csv)
    print('wrote', out_dir / 'phase3_lighting_manifest.json')
    print('wrote', out_dir / 'PHASE3_LIGHTING_ANALYSIS.md')


if __name__ == '__main__':
    main()
