#!/usr/bin/env python3
import argparse, glob, hashlib, json, os, struct
from collections import Counter

GROUPS={b'GNRL',b'TEXI',b'STRU',b'SNAP',b'VIEW',b'CTRL',b'LINK',b'OBJS',b'OBJD',b'LITS',b'LITD',b'FLAS',b'FLAD'}

def u32(b,o): return struct.unpack_from('<I',b,o)[0]
def i32(b,o): return struct.unpack_from('<i',b,o)[0]

def norm_name(s:str)->str:
    s=s.replace('\\','/')
    s=s.split('/')[-1].strip().lower()
    if s and '.' not in s: s += '.3d'
    return s

def parse_objd(payload:bytes):
    i=0; obj={'idfi':-1,'pos':(0,0,0),'ang':(0,0,0),'scale':1024}
    while i+8<=len(payload):
        n=payload[i:i+4]; ln=u32(payload,i+4); j=i+8+ln
        if j>len(payload): break
        p=i+8
        if n==b'IDFI' and ln>=4: obj['idfi']=u32(payload,p)
        elif n==b'POSI' and ln>=12: obj['pos']=(i32(payload,p),i32(payload,p+4),i32(payload,p+8))
        elif n==b'ANGS' and ln>=12: obj['ang']=(i32(payload,p),i32(payload,p+4),i32(payload,p+8))
        elif n==b'SCAL' and ln>=4:
            obj['scale']=i32(payload,p)
            if obj['scale']==0: obj['scale']=1024
        i=j
    return obj

def bs6_signature(path):
    b=open(path,'rb').read(); chunks=Counter(); rawd=Counter(); lfil=[]; objs=[]; bboxes=[]
    def walk(start,end,templates):
        i=start; local=list(templates)
        while i+8<=end:
            n=b[i:i+4]; ln=u32(b,i+4); j=i+8+ln
            if j>end: return
            payload=b[i+8:j]
            chunks[n.decode('latin1')] += 1
            if n==b'LFIL' and ln>=260:
                names=[]
                for k in range(ln//260):
                    nm=norm_name(payload[k*260:(k+1)*260].split(b'\0',1)[0].decode('latin1','ignore'))
                    if nm: names.append(nm)
                local=names; lfil.extend(names)
            elif n==b'OBJD':
                ob=parse_objd(payload); model=''
                if 0 <= ob['idfi'] < len(local): model=local[ob['idfi']]
                objs.append((ob['idfi'],ob['pos'],ob['ang'],ob['scale'],model))
            elif n==b'BBOX' and ln in (24,72): bboxes.append(ln)
            elif n==b'RAWD': rawd[ln]+=1
            if n in GROUPS: walk(i+8,j,local)
            i=j
    walk(0,len(b),[])
    return hashlib.sha1(json.dumps({'chunks':chunks,'objs':objs,'lfil':lfil,'bboxes':bboxes,'rawd':rawd},sort_keys=True,default=list).encode()).hexdigest()

def b3d_signature(path):
    b=open(path,'rb').read()
    if len(b)<64: return None
    pcount=u32(b,4); plcount=u32(b,8); pldata=u32(b,24); poff=u32(b,48); plo=u32(b,60)
    if poff+pcount*12>len(b) or pldata+plcount*24>len(b) or plo>len(b): return None
    i=plo; faces=0; pts=0; tex=Counter()
    for pi in range(plcount):
        if i+10>len(b): break
        pc=b[i]; j=i+10+pc*8
        if j>len(b): break
        if pc>=3:
            faces+=1; pts+=pc; tex[b[pldata+pi*24+4:pldata+pi*24+10].hex()] += 1
        i=j
    return hashlib.sha1(json.dumps({'p':pcount,'pl':plcount,'f':faces,'pt':pts,'tex':tex},sort_keys=True,default=list).encode()).hexdigest()

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--golden',default='batspire/research_phase1/golden_parity_signatures.json')
    args=ap.parse_args()
    g=json.load(open(args.golden))

    mismatches=[]
    for p in sorted(glob.glob('batspire/BS6_extracted/*.BS6')):
        fn=os.path.basename(p); sig=bs6_signature(p); want=g['bs6_signatures'].get(fn)
        if want!=sig: mismatches.append(('BS6',fn,want,sig))
    for p in sorted(glob.glob('batspire/3D_extracted/*.3D')):
        fn=os.path.basename(p); sig=b3d_signature(p); want=g['b3d_signatures'].get(fn)
        if want!=sig: mismatches.append(('3D',fn,want,sig))

    if mismatches:
        print('MISMATCHES',len(mismatches))
        for t,f,w,s in mismatches[:20]:
            print(t,f,'expected',w,'got',s)
        return 1

    print('OK: all signatures match golden baseline')
    print('meta',g.get('meta',{}))
    return 0

if __name__=='__main__':
    raise SystemExit(main())
