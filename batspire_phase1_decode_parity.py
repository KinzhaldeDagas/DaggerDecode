#!/usr/bin/env python3
import csv, glob, hashlib, json, os, struct
from collections import Counter, defaultdict
from dataclasses import dataclass, asdict

GROUPS={b'GNRL',b'TEXI',b'STRU',b'SNAP',b'VIEW',b'CTRL',b'LINK',b'OBJS',b'OBJD',b'LITS',b'LITD',b'FLAS',b'FLAD'}

@dataclass
class ObjInst:
    idfi:int=-1
    pos:tuple[int,int,int]=(0,0,0)
    ang:tuple[int,int,int]=(0,0,0)
    scale:int=1024


def u32(b,o): return struct.unpack_from('<I',b,o)[0]
def i32(b,o): return struct.unpack_from('<i',b,o)[0]

def norm_name(s:str)->str:
    s=s.replace('\\','/')
    s=s.split('/')[-1].strip().lower()
    if not s: return s
    if '.' not in s: s += '.3d'
    return s

def parse_objd(payload:bytes):
    i=0
    obj=ObjInst()
    while i+8<=len(payload):
        n=payload[i:i+4]; ln=u32(payload,i+4); j=i+8+ln
        if j>len(payload): break
        p=i+8
        if n==b'IDFI' and ln>=4: obj.idfi=u32(payload,p)
        elif n==b'POSI' and ln>=12: obj.pos=(i32(payload,p),i32(payload,p+4),i32(payload,p+8))
        elif n==b'ANGS' and ln>=12: obj.ang=(i32(payload,p),i32(payload,p+4),i32(payload,p+8))
        elif n==b'SCAL' and ln>=4:
            obj.scale=i32(payload,p)
            if obj.scale==0: obj.scale=1024
        i=j
    return obj

def parse_bs6(path:str, model_stems:set[str]):
    b=open(path,'rb').read()
    chunk_counts=Counter()
    rawd_lengths=Counter()
    lfil=[]
    objs=[]
    bboxes=[]

    def walk(start,end,templates):
        i=start
        local_templates=list(templates)
        while i+8<=end:
            n=b[i:i+4]; ln=u32(b,i+4); j=i+8+ln
            if j>end: return
            payload=b[i+8:j]
            key=n.decode('latin1')
            chunk_counts[key]+=1

            if n==b'LFIL' and ln>=260:
                names=[]
                for k in range(ln//260):
                    raw=payload[k*260:(k+1)*260].split(b'\0',1)[0].decode('latin1','ignore')
                    nm=norm_name(raw)
                    if nm: names.append(nm)
                local_templates=names
                lfil.extend(names)
            elif n==b'OBJD':
                ob=parse_objd(payload)
                model=''
                if 0 <= ob.idfi < len(local_templates):
                    model=local_templates[ob.idfi]
                objs.append((ob,model))
            elif n==b'BBOX' and ln in (24,72):
                bboxes.append(ln)
            elif n==b'RAWD':
                rawd_lengths[ln]+=1

            if n in GROUPS:
                walk(i+8,j,local_templates)
            i=j

    walk(0,len(b),[])

    resolved=0
    unresolved=0
    for ob,model in objs:
        stem=model.split('.')[0] if model else ''
        if stem and stem in model_stems: resolved+=1
        else: unresolved+=1

    scales=[ob.scale for ob,_ in objs] or [1024]
    pos=[ob.pos for ob,_ in objs]
    ang=[ob.ang for ob,_ in objs]
    def rng(vals,idx):
        arr=[v[idx] for v in vals] if vals else [0]
        return (min(arr),max(arr))

    sig=hashlib.sha1(json.dumps({
        'chunks':chunk_counts,
        'objs':[(ob.idfi,ob.pos,ob.ang,ob.scale,m) for ob,m in objs],
        'lfil':lfil,
        'bboxes':bboxes,
        'rawd':rawd_lengths,
    }, sort_keys=True, default=list).encode()).hexdigest()

    return {
        'file':os.path.basename(path),
        'chunk_total':sum(chunk_counts.values()),
        'objd_count':chunk_counts['OBJD'],
        'idfi_count':chunk_counts['IDFI'],
        'lfil_count':chunk_counts['LFIL'],
        'resolved_models':resolved,
        'unresolved_models':unresolved,
        'scale_min':min(scales),
        'scale_max':max(scales),
        'pos_x_min':rng(pos,0)[0],'pos_x_max':rng(pos,0)[1],
        'pos_y_min':rng(pos,1)[0],'pos_y_max':rng(pos,1)[1],
        'pos_z_min':rng(pos,2)[0],'pos_z_max':rng(pos,2)[1],
        'ang_x_min':rng(ang,0)[0],'ang_x_max':rng(ang,0)[1],
        'ang_y_min':rng(ang,1)[0],'ang_y_max':rng(ang,1)[1],
        'ang_z_min':rng(ang,2)[0],'ang_z_max':rng(ang,2)[1],
        'bbox24_count':bboxes.count(24),
        'bbox72_count':bboxes.count(72),
        'signature':sig,
        'chunk_counts':dict(chunk_counts),
        'rawd_lengths':dict(rawd_lengths),
    }

def parse_3d(path:str):
    b=open(path,'rb').read()
    if len(b)<64:
        return None
    point_count=u32(b,4); plane_count=u32(b,8); plane_data=u32(b,24); point_off=u32(b,48); plane_off=u32(b,60)
    if point_off + point_count*12 > len(b) or plane_data + plane_count*24 > len(b) or plane_off>len(b):
        return None
    i=plane_off
    faces=0
    points_total=0
    tex=Counter()
    for pi in range(plane_count):
        if i+10>len(b): break
        pc=b[i]
        j=i+10+pc*8
        if j>len(b): break
        if pc>=3:
            faces+=1
            points_total += pc
            tag=b[plane_data+pi*24+4:plane_data+pi*24+10]
            tex[tag.hex()]+=1
        i=j
    sig=hashlib.sha1(json.dumps({'p':point_count,'pl':plane_count,'f':faces,'pt':points_total,'tex':tex}, sort_keys=True, default=list).encode()).hexdigest()
    return {
        'file':os.path.basename(path),
        'point_count':point_count,
        'plane_count':plane_count,
        'face_count':faces,
        'face_point_total':points_total,
        'unique_texture_tags':len(tex),
        'signature':sig,
        'top_texture_tags':dict(tex.most_common(8)),
    }

def main():
    out_dir='batspire/research_phase1'
    os.makedirs(out_dir,exist_ok=True)

    model_stems=set(os.path.splitext(os.path.basename(p))[0].lower() for p in glob.glob('batspire/3D_extracted/*.3D'))

    bs6_rows=[]
    rawd_global=Counter()
    chunks_global=Counter()
    for p in sorted(glob.glob('batspire/BS6_extracted/*.BS6')):
        row=parse_bs6(p,model_stems)
        bs6_rows.append(row)
        chunks_global.update(row['chunk_counts'])
        rawd_global.update(row['rawd_lengths'])

    d3_rows=[]
    for p in sorted(glob.glob('batspire/3D_extracted/*.3D')):
        r=parse_3d(p)
        if r: d3_rows.append(r)

    with open(f'{out_dir}/bs6_diagnostics.csv','w',newline='') as f:
        cols=[k for k in bs6_rows[0].keys() if k not in ('chunk_counts','rawd_lengths')]
        w=csv.DictWriter(f,fieldnames=cols); w.writeheader()
        for r in bs6_rows:
            w.writerow({k:r[k] for k in cols})

    with open(f'{out_dir}/b3d_diagnostics.csv','w',newline='') as f:
        cols=[k for k in d3_rows[0].keys() if k!='top_texture_tags']
        w=csv.DictWriter(f,fieldnames=cols); w.writeheader()
        for r in d3_rows:
            w.writerow({k:r[k] for k in cols})

    with open(f'{out_dir}/rawd_research_table.csv','w',newline='') as f:
        w=csv.writer(f)
        w.writerow(['rawd_length','count','hypothesis'])
        for ln,count in sorted(rawd_global.items()):
            hyp='unknown'
            if ln==8: hyp='likely compact params (id/value pairs?)'
            elif ln==24: hyp='vector-ish triplets/transform adjunct'
            elif ln==32: hyp='material/control payload candidate'
            elif ln==72: hyp='bbox/full extents companion candidate'
            w.writerow([ln,count,hyp])

    golden={
        'meta':{
            'bs6_files':len(bs6_rows),
            'b3d_files':len(d3_rows),
        },
        'bs6_signatures':{r['file']:r['signature'] for r in bs6_rows},
        'b3d_signatures':{r['file']:r['signature'] for r in d3_rows},
        'global_chunk_counts':dict(chunks_global),
        'global_rawd_lengths':dict(rawd_global),
    }
    with open(f'{out_dir}/golden_parity_signatures.json','w') as f:
        json.dump(golden,f,indent=2,sort_keys=True)

    print('wrote',out_dir)
    print('bs6',len(bs6_rows),'b3d',len(d3_rows))

if __name__=='__main__':
    main()
