#!/usr/bin/env python3
import csv
import os
import re
from collections import Counter, defaultdict
from pathlib import Path

IN_BIND='batspire/research_phase2/texture_tag_bindings.csv'
IN_TEX='batspire/research_phase2/texture_tag_inventory.csv'
IN_BSI='batspire/research_phase2/bsi_filename_inventory.csv'
OUT_DIR='batspire/research_phase2'


def family(s:str)->str:
    s=s.lower()
    m=re.match(r'([a-z]+|\d+)', s)
    return m.group(1) if m else s[:1]


def md_cell(value: str) -> str:
    return value.replace('|', '\\|')


def main():
    binds=list(csv.DictReader(open(IN_BIND,newline='')))
    tex={r['texture_tag_hex']:r for r in csv.DictReader(open(IN_TEX,newline=''))}
    bsi=list(csv.DictReader(open(IN_BSI,newline='')))

    bsi_families=Counter()
    bsi_by_family=defaultdict(list)
    for r in bsi:
        stem=r.get('stem_lower','').strip()
        if not stem:
            continue
        fam=family(stem)
        bsi_families[fam]+=1
        if len(bsi_by_family[fam])<12:
            bsi_by_family[fam].append(stem)

    rows=[]
    for r in binds:
        tag=r['texture_tag_hex']
        t=tex.get(tag)
        sample_models=(t.get('sample_models','') if t else '')
        stems=[]
        for token in sample_models.split(' | '):
            token=token.strip()
            if not token:
                continue
            stem,_=os.path.splitext(token)
            stems.append(stem.lower())

        fam_counter=Counter(family(s) for s in stems)
        dom_fam=fam_counter.most_common(1)[0][0] if fam_counter else ''
        fam_candidates=bsi_by_family.get(dom_fam,[])

        rows.append({
            'texture_tag_hex':tag,
            'face_count':int(r['face_count']),
            'model_count':int(r['model_count']),
            'status':r['status'],
            'dominant_model_family':dom_fam,
            'family_model_hits':fam_counter.get(dom_fam,0),
            'bsi_family_count':bsi_families.get(dom_fam,0),
            'bsi_family_examples':' | '.join(fam_candidates[:8]),
            'sample_models':sample_models,
        })

    rows.sort(key=lambda x:x['face_count'], reverse=True)

    out_csv=Path(OUT_DIR)/'texture_tag_bindings_family_candidates.csv'
    with open(out_csv,'w',newline='') as f:
        cols=['texture_tag_hex','face_count','model_count','status','dominant_model_family','family_model_hits','bsi_family_count','bsi_family_examples','sample_models']
        w=csv.DictWriter(f,fieldnames=cols)
        w.writeheader()
        w.writerows(rows)

    total_faces=sum(r['face_count'] for r in rows)
    nonzero_faces=sum(r['face_count'] for r in rows if r['texture_tag_hex']!='000000000000')
    unbound_faces=sum(r['face_count'] for r in rows if r['status']=='unbound')
    bound_faces=sum(r['face_count'] for r in rows if r['status'].startswith('bound_'))
    bound_nonzero_faces=sum(r['face_count'] for r in rows if r['texture_tag_hex']!='000000000000' and r['status'].startswith('bound_'))
    zero_tag_faces=next((r['face_count'] for r in rows if r['texture_tag_hex']=='000000000000'),0)
    with_family_candidates=sum(r['face_count'] for r in rows if r['bsi_family_count']>0)

    md=[]
    md.append('# Texture Tag Binding Analysis (Phase 2)')
    md.append('')
    md.append(f'- total tags: **{len(rows)}**')
    md.append(f'- total faces: **{total_faces}**')
    md.append(f'- non-zero-tag faces: **{nonzero_faces}**')
    md.append(f'- bound faces: **{bound_faces}** (**{(100.0*bound_faces/total_faces):.2f}%** of total)')
    md.append(f'- bound non-zero-tag faces: **{bound_nonzero_faces}** (**{(100.0*bound_nonzero_faces/nonzero_faces):.2f}%** of non-zero)')
    md.append(f'- unbound faces: **{unbound_faces}**')
    md.append(f'- fallback-zero-tag faces: **{zero_tag_faces}**')
    md.append(f'- faces with non-empty BSI family candidates: **{with_family_candidates}** (**{(100.0*with_family_candidates/total_faces):.2f}%**)')
    md.append('')
    md.append('## Key finding')
    md.append('Direct model-stem to BSI-stem matching is mostly sparse; family-level heuristics provide candidate clusters but not deterministic bindings.')
    md.append('')
    md.append('## Top tags with family candidates')
    md.append('| tag | face_count | dominant_family | bsi_family_count | bsi_examples |')
    md.append('|---|---:|---|---:|---|')
    shown=0
    for r in rows:
        if r['bsi_family_count']<=0:
            continue
        md.append(
            f"| `{r['texture_tag_hex']}` | {r['face_count']} | `{r['dominant_model_family']}` | {r['bsi_family_count']} | {md_cell(r['bsi_family_examples'])} |"
        )
        shown+=1
        if shown>=20:
            break

    md.append('')
    md.append('## Recommended hardening steps')
    md.append('1. Add explicit per-level context key (`level + tag`) to binding table to reduce global collisions.')
    md.append('2. Add manual-reviewed bindings for highest non-zero face-count tags first.')
    md.append('3. Add coverage gate script to require increasing bound_face_ratio over time.')

    out_md=Path(OUT_DIR)/'PHASE2_BINDING_ANALYSIS.md'
    out_md.write_text('\n'.join(md))

    print('wrote',out_csv)
    print('wrote',out_md)


if __name__=='__main__':
    main()
