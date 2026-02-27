#!/usr/bin/env python3
import csv, glob, json, os, struct
from collections import Counter, defaultdict
from pathlib import Path

GROUPS={b'GNRL',b'TEXI',b'STRU',b'SNAP',b'VIEW',b'CTRL',b'LINK',b'OBJS',b'OBJD',b'LITS',b'LITD',b'FLAS',b'FLAD'}

def u32(b,o): return struct.unpack_from('<I',b,o)[0]

def parse_bs6_texi(path):
    b=Path(path).read_bytes()
    tex_dirs=[]
    def walk(start,end):
        i=start
        while i+8<=end:
            n=b[i:i+4]; ln=u32(b,i+4); j=i+8+ln
            if j>end: return
            payload=b[i+8:j]
            if n==b'TEXI':
                k=0
                while k+8<=len(payload):
                    cn=payload[k:k+4]; clen=u32(payload,k+4); kj=k+8+clen
                    if kj>len(payload): break
                    if cn==b'DIRN' and clen>0:
                        s=payload[k+8:kj].split(b'\0',1)[0].decode('latin1','ignore').strip()
                        if s: tex_dirs.append(s)
                    k=kj
            if n in GROUPS:
                walk(i+8,j)
            i=j
    walk(0,len(b))
    return tex_dirs

def parse_3d_texture_tags(path):
    b=Path(path).read_bytes()
    if len(b)<64: return []
    plane_count=u32(b,8); plane_data=u32(b,24); plane_off=u32(b,60)
    if plane_data + plane_count*24 > len(b) or plane_off>len(b): return []
    i=plane_off
    tags=[]
    for pi in range(plane_count):
        if i+10>len(b): break
        pc=b[i]
        j=i+10+pc*8
        if j>len(b): break
        if pc>=3:
            tag=b[plane_data+pi*24+4:plane_data+pi*24+10].hex()
            tags.append(tag)
        i=j
    return tags

def scan_bsi_names(root='batspire/bsi_extracted'):
    names=[]
    for p in glob.glob(f'{root}/*'):
        bn=os.path.basename(p)
        stem,ext=os.path.splitext(bn)
        names.append({'file':bn,'stem_lower':stem.lower(),'ext':ext.lower()})
    return names

def main():
    out='batspire/research_phase2'
    os.makedirs(out,exist_ok=True)

    # BS6 TEXI directory hints
    bs6_rows=[]
    texi_counter=Counter()
    for p in sorted(glob.glob('batspire/BS6_extracted/*.BS6')):
        dirs=parse_bs6_texi(p)
        if not dirs:
            bs6_rows.append({'file':os.path.basename(p),'texi_dir_count':0,'texi_dirs':''})
            continue
        for d in dirs: texi_counter[d]+=1
        bs6_rows.append({'file':os.path.basename(p),'texi_dir_count':len(dirs),'texi_dirs':' | '.join(dirs)})

    with open(f'{out}/bs6_texi_directories.csv','w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=['file','texi_dir_count','texi_dirs'])
        w.writeheader(); w.writerows(bs6_rows)

    # 3D tag inventory
    tag_models=defaultdict(set)
    tag_faces=Counter()
    for p in sorted(glob.glob('batspire/3D_extracted/*.3D')):
        bn=os.path.basename(p)
        tags=parse_3d_texture_tags(p)
        for t in tags:
            tag_faces[t]+=1
            tag_models[t].add(bn)

    with open(f'{out}/texture_tag_inventory.csv','w',newline='') as f:
        w=csv.writer(f)
        w.writerow(['texture_tag_hex','face_count','model_count','sample_models'])
        for tag,count in tag_faces.most_common():
            models=sorted(tag_models[tag])
            w.writerow([tag,count,len(models),' | '.join(models[:8])])

    # BSI filenames inventory as texture-container candidate index
    bsi=scan_bsi_names()
    with open(f'{out}/bsi_filename_inventory.csv','w',newline='') as f:
        w=csv.DictWriter(f,fieldnames=['file','stem_lower','ext'])
        w.writeheader(); w.writerows(sorted(bsi,key=lambda r:r['file']))

    # Phase2 analysis markdown
    top_tags=tag_faces.most_common(20)
    md=[]
    md.append('# Phase 2 Texture Pipeline Research Inventory')
    md.append('')
    md.append('## Corpus signals')
    md.append(f'- BS6 files scanned: **{len(bs6_rows)}**')
    md.append(f'- Distinct TEXI directory hints: **{len(texi_counter)}**')
    md.append(f'- 3D models scanned: **{len(glob.glob("batspire/3D_extracted/*.3D"))}**')
    md.append(f'- Distinct texture tags observed: **{len(tag_faces)}**')
    md.append('')
    md.append('## Most common texture tags (face-weighted)')
    md.append('| tag | face_count | model_count |')
    md.append('|---|---:|---:|')
    for tag,count in top_tags:
        md.append(f'| `{tag}` | {count} | {len(tag_models[tag])} |')
    md.append('')
    md.append('## TEXI directory hints (top)')
    md.append('| texi_dir | bs6_count |')
    md.append('|---|---:|')
    for d,c in texi_counter.most_common(12):
        md.append(f'| `{d}` | {c} |')
    md.append('')
    md.append('## Interpretation / hardening notes')
    md.append('1. Texture tags are highly reused across many models; mapping likely requires additional tables beyond the 6-byte tag alone.')
    md.append('2. BS6 TEXI `DIRN` strongly indicates texture-cell roots; normalize path casing and DOS roots during runtime resolution.')
    md.append('3. `bsi_extracted` inventory should be used as candidate texture namespace while reverse-engineering tag->asset mapping.')
    md.append('')
    md.append('## Output files')
    md.append('- `bs6_texi_directories.csv`')
    md.append('- `texture_tag_inventory.csv`')
    md.append('- `bsi_filename_inventory.csv`')

    Path(f'{out}/PHASE2_TEXTURE_INVENTORY.md').write_text('\n'.join(md))

    manifest={
        'meta':{
            'bs6_files':len(bs6_rows),
            'distinct_texi_dirs':len(texi_counter),
            'models_3d':len(glob.glob('batspire/3D_extracted/*.3D')),
            'distinct_texture_tags':len(tag_faces),
        },
        'top_texture_tags':[{ 'tag':t,'face_count':c,'model_count':len(tag_models[t]) } for t,c in top_tags],
        'top_texi_dirs':[{ 'dir':d,'count':c } for d,c in texi_counter.most_common(20)],
    }
    Path(f'{out}/phase2_texture_manifest.json').write_text(json.dumps(manifest,indent=2))

    print('wrote',out)
    print('distinct texture tags',len(tag_faces))

if __name__=='__main__':
    main()
