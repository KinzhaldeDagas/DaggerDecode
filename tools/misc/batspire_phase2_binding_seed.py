#!/usr/bin/env python3
import csv, json, os
from collections import Counter
from pathlib import Path

IN_TAGS='batspire/research_phase2/texture_tag_inventory.csv'
IN_BSI='batspire/research_phase2/bsi_filename_inventory.csv'
OUT_DIR='batspire/research_phase2'


def load_rows(path):
    with open(path,newline='') as f:
        return list(csv.DictReader(f))


def model_stems_from_samples(sample_models):
    out=[]
    for token in sample_models.split(' | '):
        t=token.strip()
        if not t: continue
        stem,_=os.path.splitext(t)
        out.append(stem.lower())
    return out


def main():
    tags=load_rows(IN_TAGS)
    bsi=load_rows(IN_BSI)
    bsi_stems=set(r['stem_lower'] for r in bsi if r.get('stem_lower'))

    bindings=[]
    faces_total=0
    faces_bound=0

    for r in tags:
        face_count=int(r['face_count'])
        faces_total += face_count
        tag=r['texture_tag_hex']

        samples=model_stems_from_samples(r.get('sample_models',''))
        candidate_stems=[s for s in samples if s in bsi_stems]
        candidate_stems=list(dict.fromkeys(candidate_stems))[:8]

        # conservative default: keep unbound until manually validated
        status='unbound'
        bound_stem=''
        confidence='none'

        if tag == '000000000000':
            status='fallback_unknown'

        row={
            'texture_tag_hex':tag,
            'status':status,
            'bound_bsi_stem':bound_stem,
            'confidence':confidence,
            'face_count':face_count,
            'model_count':int(r['model_count']),
            'candidate_bsi_stems':' | '.join(candidate_stems),
            'notes':''
        }
        bindings.append(row)

    out_csv=Path(OUT_DIR)/'texture_tag_bindings.csv'
    with open(out_csv,'w',newline='') as f:
        cols=['texture_tag_hex','status','bound_bsi_stem','confidence','face_count','model_count','candidate_bsi_stems','notes']
        w=csv.DictWriter(f,fieldnames=cols)
        w.writeheader(); w.writerows(bindings)

    status_counts=Counter(r['status'] for r in bindings)
    manifest={
        'total_tags':len(bindings),
        'total_faces':faces_total,
        'bound_faces':faces_bound,
        'bound_face_ratio': 0.0 if faces_total==0 else (faces_bound/faces_total),
        'status_counts':dict(status_counts)
    }
    out_json=Path(OUT_DIR)/'phase2_binding_coverage.json'
    out_json.write_text(json.dumps(manifest,indent=2,sort_keys=True))

    out_md=Path(OUT_DIR)/'PHASE2_BINDING_SEED.md'
    md=[]
    md.append('# Phase 2 Binding Seed')
    md.append('')
    md.append('This artifact seeds `texture_tag_bindings.csv` for manual/iterative tag->BSI mapping.')
    md.append('')
    md.append(f"- total tags: **{manifest['total_tags']}**")
    md.append(f"- total faces represented: **{manifest['total_faces']}**")
    md.append(f"- bound faces: **{manifest['bound_faces']}** (ratio **{manifest['bound_face_ratio']:.2%}**)" )
    md.append(f"- status counts: `{manifest['status_counts']}`")
    md.append('')
    md.append('## Next actions')
    md.append('1. Manually confirm high-frequency non-zero tags and assign `bound_bsi_stem`.')
    md.append('2. Re-run this script + add an assert script to require monotonically increasing bound coverage.')
    md.append('3. Use per-level context to disambiguate tags reused across many models.')
    out_md.write_text('\n'.join(md))

    print('wrote', out_csv)
    print('wrote', out_json)
    print('wrote', out_md)


if __name__=='__main__':
    main()
