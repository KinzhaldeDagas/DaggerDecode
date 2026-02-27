#!/usr/bin/env python3
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path

IN_INV = Path('batspire/research_phase3/lighting_chunk_inventory.csv')
IN_LEN = Path('batspire/research_phase3/lighting_payload_lengths.csv')
IN_MANIFEST = Path('batspire/research_phase3/phase3_lighting_manifest.json')
OUT_DIR = Path('batspire/research_phase3')


def load_csv(path: Path):
    with path.open(newline='') as f:
        return list(csv.DictReader(f))


def pearson(xs, ys):
    n = len(xs)
    if n == 0:
        return 0.0
    mx = sum(xs) / n
    my = sum(ys) / n
    num = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    denx = sum((x - mx) ** 2 for x in xs)
    deny = sum((y - my) ** 2 for y in ys)
    if denx <= 0 or deny <= 0:
        return 0.0
    return num / ((denx * deny) ** 0.5)


def main():
    inv = load_csv(IN_INV)
    lengths = load_csv(IN_LEN)
    manifest = json.loads(IN_MANIFEST.read_text())

    chunk_cols = [c for c in inv[0].keys() if c.endswith('_count')]
    totals = {c: sum(int(r[c]) for r in inv) for c in chunk_cols}

    # Per-file density and outliers
    file_totals = []
    for r in inv:
        lighting_total = sum(int(r[c]) for c in chunk_cols if c != 'rawd_count')
        rawd = int(r['rawd_count'])
        file_totals.append({
            'file': r['file'],
            'lighting_total': lighting_total,
            'rawd_count': rawd,
            'rawd_to_lighting_ratio': (rawd / lighting_total) if lighting_total else 0.0,
        })
    file_totals.sort(key=lambda x: x['lighting_total'], reverse=True)

    # Correlation probes
    litd = [int(r['litd_count']) for r in inv]
    brit = [int(r['brit_count']) for r in inv]
    ambi = [int(r['ambi_count']) for r in inv]
    flad = [int(r['flad_count']) for r in inv]
    rawd = [int(r['rawd_count']) for r in inv]

    corr = {
        'litd_vs_brit': pearson(litd, brit),
        'litd_vs_ambi': pearson(litd, ambi),
        'flad_vs_litd': pearson(flad, litd),
        'rawd_vs_litd': pearson(rawd, litd),
        'rawd_vs_ambi': pearson(rawd, ambi),
    }

    # Payload-size concentration by chunk
    by_chunk = defaultdict(list)
    for r in lengths:
        by_chunk[r['chunk']].append((int(r['payload_len']), int(r['count'])))

    concentration_rows = []
    decode_priority_rows = []
    for chunk, vals in sorted(by_chunk.items()):
        vals.sort(key=lambda t: t[1], reverse=True)
        total = sum(c for _, c in vals)
        top_len, top_count = vals[0]
        top_share = top_count / total if total else 0.0
        top3 = vals[:3]
        top3_share = sum(c for _, c in top3) / total if total else 0.0
        concentration_rows.append({
            'chunk': chunk,
            'total_rows': total,
            'distinct_payload_lens': len(vals),
            'top_payload_len': top_len,
            'top_payload_count': top_count,
            'top_payload_share': round(top_share, 6),
            'top3_payload_share': round(top3_share, 6),
        })

        for rank, (ln, cnt) in enumerate(top3, start=1):
            decode_priority_rows.append({
                'chunk': chunk,
                'rank_within_chunk': rank,
                'payload_len': ln,
                'count': cnt,
                'share_within_chunk': round((cnt / total) if total else 0.0, 6),
            })

    # Write CSV artifacts
    out_conc = OUT_DIR / 'phase3_payload_concentration.csv'
    with out_conc.open('w', newline='') as f:
        cols = ['chunk', 'total_rows', 'distinct_payload_lens', 'top_payload_len', 'top_payload_count', 'top_payload_share', 'top3_payload_share']
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(concentration_rows)

    out_priority = OUT_DIR / 'phase3_decode_priority.csv'
    with out_priority.open('w', newline='') as f:
        cols = ['chunk', 'rank_within_chunk', 'payload_len', 'count', 'share_within_chunk']
        w = csv.DictWriter(f, fieldnames=cols)
        w.writeheader()
        w.writerows(decode_priority_rows)

    # Markdown analysis
    md = []
    md.append('# Phase 3 Lighting/Material CSV Analysis')
    md.append('')
    md.append(f"- BS6 files analyzed: **{len(inv)}**")
    md.append(f"- Total lighting chunks (excl. RAWD): **{sum(v for k,v in totals.items() if k != 'rawd_count')}**")
    md.append(f"- Total RAWD chunks: **{totals['rawd_count']}**")
    md.append('')
    md.append('## Correlation probes (file-level counts)')
    md.append('| pair | r |')
    md.append('|---|---:|')
    for k, v in corr.items():
        md.append(f'| `{k}` | {v:.4f} |')
    md.append('')
    md.append('## Highest lighting-density files')
    md.append('| file | lighting_total | rawd_count | rawd_to_lighting_ratio |')
    md.append('|---|---:|---:|---:|')
    for r in file_totals[:15]:
        md.append(f"| `{r['file']}` | {r['lighting_total']} | {r['rawd_count']} | {r['rawd_to_lighting_ratio']:.3f} |")
    md.append('')
    md.append('## Payload concentration highlights')
    md.append('| chunk | distinct_lens | top_len | top_share | top3_share |')
    md.append('|---|---:|---:|---:|---:|')
    for r in concentration_rows:
        md.append(
            f"| `{r['chunk']}` | {r['distinct_payload_lens']} | {r['top_payload_len']} | {r['top_payload_share']:.3f} | {r['top3_payload_share']:.3f} |"
        )
    md.append('')
    md.append('## Phase 3 decode order (recommended)')
    md.append('1. **AMBI (len=4)** and **BRIT (len=4)** first: single-shape payloads and direct brightness/ambient candidates.')
    md.append('2. **FLAD** next: dominant payload lens likely map to repeated light definition struct variants.')
    md.append('3. **LITD/LITS** after FLAD schema confidence improves; use count alignment with BRIT as a validation signal.')
    md.append('4. **RAWD** last for material/flag interpretation; prioritize levels with highest RAWD:lighting ratio for contrastive diffing.')

    out_md = OUT_DIR / 'PHASE3_CSV_ANALYSIS.md'
    out_md.write_text('\n'.join(md) + '\n')

    summary = {
        'files': len(inv),
        'totals': totals,
        'correlations': corr,
        'top_lighting_files': file_totals[:15],
        'payload_concentration': concentration_rows,
        'source_manifest': manifest,
    }
    (OUT_DIR / 'phase3_csv_analysis_summary.json').write_text(json.dumps(summary, indent=2, sort_keys=True) + '\n')

    print('wrote', out_conc)
    print('wrote', out_priority)
    print('wrote', out_md)
    print('wrote', OUT_DIR / 'phase3_csv_analysis_summary.json')


if __name__ == '__main__':
    main()
