"""File-only, reproducible Kamui name recovery. No game assets or online sources.

Run: python recover.py --stage targeted
Compiler can be selected with --cc; outputs are written beside this script.
Every native search includes four XOR decoys with identical candidate space.
Placeholder lengths are deliberately NOT used as search filters.
"""
import t8names_paths as _P
import argparse
import collections as co
import csv
import difflib
import hashlib
import json
import math
from pathlib import Path
import re
import random
import subprocess
import sys
import time

HERE = _P.RECOVERY
DATA = HERE.parent
from kamuiHash import getKamuiHashDigest as digest

MASKS = (0, 0x13371337, 0x5A5A5A5A, 0xA5A5A5A5, 0x0F0F0F0F)
NEIGH = 'neighbour_anims(id-2..id+2)'

def read(name):
    with (DATA / name).open(encoding='utf-8-sig', newline='') as f:
        return list(csv.DictReader(f))

def write_csv(path, rows, fields):
    with path.open('w', encoding='utf-8', newline='') as f:
        w = csv.DictWriter(f, fields, extrasaction='ignore')
        w.writeheader(); w.writerows(rows)

def body(move):
    return move.split('_', 1)[-1].strip('_')

def variants(move):
    raw = body(move)
    out = {raw, raw.lower(), re.sub(r'(?<=[a-z])(?=[A-Z])', '_', raw).lower()}
    for x in list(out):
        out.update((x.replace('_', ''), re.sub(r'\d+$', '', x).rstrip('_')))
    return {x for x in out if x}

class Recovery:
    def __init__(self, cc):
        self.known = read('known_anim_names.csv')
        self.moves = read('known_move_to_anim.csv')
        self.unknown = read('unresolved_anim_hashes.csv')
        self.byhash = {int(r['anim_hash'],16): r for r in self.unknown}
        self.names = {r['anim_name'] for r in self.known}
        self.codes = {r['code'] for r in self.moves}
        self.first = co.Counter(); self.cats = co.Counter(); self.initials = co.Counter()
        self.local = co.defaultdict(co.Counter); self.pc = co.defaultdict(co.Counter)
        self.rests = co.defaultdict(set); self.suffix = co.Counter()
        for n in sorted(self.names):
            p = n.split('_',2)
            if len(p) != 3: continue
            f,c,r = p; self.first[f]+=1; self.cats[c]+=1; self.rests[c].add(r)
            self.pc[f][c]+=1
            if len(f)==5 and f[:3] in self.codes:
                self.initials[f[3:]]+=1; self.local[f[:3]][f]+=1
            for mt in re.finditer(r'_|(?<=\D)(?=\d+$)', r):
                self.suffix[r[mt.start():]]+=1
        self.shared = {f for f in self.first if not (len(f)==5 and f[:3] in self.codes)}
        self.examples = co.defaultdict(set)
        self.rules = co.defaultdict(set)
        self.samebody = co.defaultdict(set)
        for m in self.moves:
            p = m['anim_name'].split('_',2)
            if len(p)!=3 or not m['move_name']: continue
            a = body(m['move_name']).lower(); b = p[2]
            self.samebody[a].add(m['anim_name'])
            if len(p[0]) != 5 or p[0][:3] != m['code']: continue
            sm=difflib.SequenceMatcher(None,a,b.lower(),autojunk=False)
            mt=sm.find_longest_match()
            if mt.size<3:continue
            rule=(a[:mt.a],a[mt.a+mt.size:],b[:mt.b],b[mt.b+mt.size:])
            if len(rule[0])+len(rule[1])>30:continue
            self.rules[rule].add((m['code'],a,b))
        self.rules = {k:v for k,v in self.rules.items() if len({x[1] for x in v})>=3}
        self.exe=HERE/'kamui_search.exe'
        if not self.exe.exists() or self.exe.stat().st_mtime<(_P.SRC/'kamui_search.c').stat().st_mtime:
            subprocess.run([cc,'-O3','-o',str(self.exe),str(_P.SRC/'kamui_search.c')],check=True)
        self.jobs=HERE/'jobs';self.jobs.mkdir(exist_ok=True)

    def search(self, stage, label, targets, lists):
        lists=[sorted(v if isinstance(v,set) else set(v)) for v in lists]
        if any(not l for l in lists):return []
        total=math.prod(map(len,lists))
        targets=sorted(set(targets))
        targetmap=co.defaultdict(list)
        for i,mask in enumerate(MASKS):
            for h in targets:targetmap[h^mask].append((i,h))
        pre=self.jobs/f'{stage}_{label}'
        tf=pre.with_suffix('.targets')
        tf.write_text(''.join(f'{h:08X} 0\n' for h in sorted(targetmap)),encoding='ascii')
        files=[]
        for i,l in enumerate(lists):
            f=pre.with_suffix(f'.{i}.txt');f.write_text('\n'.join(l)+'\n',encoding='ascii');files.append(str(f))
        ts=time.perf_counter()
        p=subprocess.run([str(self.exe),str(tf),*files],capture_output=True,text=True,check=True)
        elapsed=time.perf_counter()-ts
        hits=[];counts=[0]*5
        for line in p.stdout.splitlines():
            hh,n=line.split('\t');actual=int(hh,16)
            for i,h in targetmap[actual]:
                counts[i]+=1
                hits.append(dict(stage=stage,job=label,decoy=i,hash=f'0x{h:08X}',name=n,
                                 placeholder_length=len(self.byhash[h]['placeholder']),name_length=len(n)))
        stats=dict(stage=stage,job=label,targets=len(targets),candidates=total,
                   expected_chance=total*len(targets)/2**32,real_hits=counts[0],
                   decoy_1=counts[1],decoy_2=counts[2],decoy_3=counts[3],decoy_4=counts[4],
                   decoy_mean=sum(counts[1:])/4,margin=counts[0]-sum(counts[1:])/4,
                   seconds=round(elapsed,3))
        for hit in hits: hit.update(expected_chance=stats['expected_chance'],decoy_mean=stats['decoy_mean'])
        with (HERE/f'{stage}_stats.jsonl').open('a',encoding='utf-8') as f:f.write(json.dumps(stats)+'\n')
        with (HERE/f'{stage}_hits.jsonl').open('a',encoding='utf-8') as f:
            for hit in hits:f.write(json.dumps(hit)+'\n')
        return hits

    def preflight(self):
        assert all(digest(r['anim_name'])==int(r['hash'],16) for r in self.known)
        p=subprocess.run([str(self.exe),'--hash'],input='\n'.join(sorted(self.names))+'\n',capture_output=True,text=True,check=True)
        assert all(digest(n)==int(h,16) for h,n in (l.split(' ',1) for l in p.stdout.splitlines()))
        rng=random.Random(20261115)
        boundaries=[''.join(rng.choice('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789') for _ in range(n)) for n in range(81) for i in range(10)]
        q=subprocess.run([str(self.exe),'--hash'],input='\n'.join(boundaries)+'\n',capture_output=True,text=True,check=True)
        assert len(q.stdout.splitlines())==len(boundaries)
        assert all(digest(n)==int(h,16) for h,n in (l.split(' ',1) for l in q.stdout.splitlines()))
        specials=['','None','none','NULL','null','0','-1','invalid','dummy','motOrigin']
        r=dict(known_python_verified=len(self.known),known_c_verified=len(p.stdout.splitlines()),ascii_c_boundary_checks=len(boundaries),
               special_hashes={s:f'0x{digest(s):08X}' for s in specials},
               input_sha256={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in DATA.glob('*') if f.is_file()},
               rules=len(self.rules),initials=list(self.initials),suffixes=len(self.suffix))
        (HERE/'preflight.json').write_text(json.dumps(r,indent=2),encoding='utf-8')
        write_csv(HERE/'learned_rules.csv', [dict(before=k[0],after=k[1],new_before=k[2],new_after=k[3],support=len(v),examples=repr(sorted(v)[:4])) for k,v in sorted(self.rules.items())], ['before','after','new_before','new_after','support','examples'])
        print('preflight',r['known_python_verified'],r['known_c_verified'],'rules',len(self.rules),flush=True)

    def targeted(self):
        # Slot rewriting rules are learned before inspecting any search hits.
        suffixes={s for s,count in self.suffix.items() if count>=8 and len(s)<=12}
        suffixes.update(('', '_f','_y','_n','_end','_loop','_start', '00','01','02','03','1','2','3','_00','_01','_02'))
        for ix,(h,row) in enumerate(self.byhash.items()):
            codes=row['codes'].split(); shared=len(codes)>12
            moves=row['move_names'].split(' | ') if row['move_names'] else []
            if not moves: continue
            rests=set();trace=co.defaultdict(set)
            for m in moves:
                vv=variants(m)
                for v in vv:trace[v].add(f'direct:{m}')
                a=body(m).lower()
                for (start,end,ns,ne),examples in self.rules.items():
                    if not a.startswith(start) or (end and not a.endswith(end)):continue
                    middle=a[len(start):len(a)-len(end) if end else len(a)]
                    if len(middle)<2:continue
                    v=ns+middle+ne
                    if len(v)>64:continue
                    vv.add(v);trace[v].add(f'learned:{m}:{start!r},{end!r}->{ns!r},{ne!r}')
                for v in list(vv):
                    for s in suffixes:
                        n=v+s
                        rests.add(n)
                        if s:trace[n].add(f'suffix:{m}:{v}+{s}')
                for n in self.samebody[a]:
                    rest=n.split('_',2)[2]; rests.add(rest);trace[rest].add(f'same_move:{m}:{n}')
            prefixes=set(self.shared if shared else [])
            if not shared:
                for code in codes:
                    prefixes.update(code+i for i in self.initials)
                    prefixes.add(code)
            cats={'co','at','un','th','dm','gd','ra','ro'}
            if shared:cats={'co','dm','un','gd','at','th'}
            pc={p+'_'+c+'_' for p in prefixes for c in cats}
            hits=self.search('targeted',f'{h:08X}',[h],[pc,rests])
            for hit in hits:
                if hit['decoy']==0:
                    hit['trace']=sorted(trace[hit['name'].split('_',2)[-1]])
                    print('HIT',hit,flush=True)
                    with (HERE/'targeted_evidence.jsonl').open('a',encoding='utf-8') as f:f.write(json.dumps(hit)+'\n')
            if ix%40==0:print('targeted progress',ix,'/',len(self.byhash),flush=True)

    def families(self):
        # Same prefix/category, one token or number modified; observed token contexts.
        contexts=co.defaultdict(set)
        parts={n:n.split('_') for n in self.names}
        for n,p in parts.items():
            if len(p)<3:continue
            for i in range(2,len(p)):
                key=(p[1],tuple(p[2:i]),tuple(p[i+1:]))
                contexts[key].add(p[i])
        for code in ['shared']+sorted(self.codes):
            targets=[h for h,r in self.byhash.items() if (len(r['codes'].split())>12 if code=='shared' else r['codes']==code)]
            if not targets:continue
            seeds={n for n in self.names if (n.split('_')[0] in self.shared if code=='shared' else n.startswith(code))}
            for h in targets:seeds.update(filter(None,self.byhash[h][NEIGH].split(' | ')))
            candidates=set();trace=co.defaultdict(set)
            for n in seeds:
                variants_n={n};p=n.split('_')
                if len(p)>=3:
                    for i in range(2,len(p)):
                        key=(p[1],tuple(p[2:i]),tuple(p[i+1:]))
                        for token in contexts[key]:
                            q=p.copy();q[i]=token;variants_n.add('_'.join(q))
                    if p[-1] in {'f','y','n','l','r','L','R','left','right'}:
                        for t in ('f','y','n','l','r','L','R','left','right'):variants_n.add('_'.join(p[:-1]+[t]))
                for match in re.finditer(r'\d+',n):
                    for i in range(21):variants_n.add(n[:match.start()]+str(i).zfill(len(match[0]))+n[match.end():])
                for s in ('_f','_y','_n','_l','_r','_L','_R','_b','_end','_loop','_start','_short','_long','_00','_01','_02','00','01','02','2','3'):
                    variants_n.add(n+s)
                for v in variants_n:
                    if v in self.names:continue
                    candidates.add(v);trace[v].add(n)
            hits=self.search('families',code,targets,[candidates])
            for hit in hits:
                if hit['decoy']==0:
                    hit['seeds']=sorted(trace[hit['name']]);print('FAMILY',hit,flush=True)
                    with (HERE/'families_evidence.jsonl').open('a',encoding='utf-8') as f:f.write(json.dumps(hit)+'\n')

    def supported_seeds(self):
        seeds=set(self.names)
        for r in read('baseline_vetted.csv'):
            if r['status']=='confirmed':seeds.add(r['anim_name'])
        for line in (HERE/'targeted_evidence.jsonl').read_text().splitlines():
            r=json.loads(line)
            if not r['placeholder_length'] or r['placeholder_length']==r['name_length']:
                seeds.add(r['name'])
        return seeds

    def companions(self):
        seeds=self.supported_seeds()
        # Add/remove a suffix, change a number, or take an observed initial/category.
        for code in ['shared']+sorted(self.codes):
            targets=[h for h,r in self.byhash.items() if (len(r['codes'].split())>12 if code=='shared' else r['codes']==code)]
            if not targets:continue
            local={n for n in seeds if (n.split('_')[0] in self.shared if code=='shared' else n.startswith(code))}
            initials={n.split('_')[0] for n in local};candidates=set();traces=co.defaultdict(set)
            for n in sorted(local):
                p=n.split('_',2)
                if len(p)!=3:continue
                pp,c,r=p
                rests={r}
                for suff in ('_c','_f','_y','_n','_l','_r','_s','_start','_end','_loop','_po1','_po2','_po3','_d1','_d2'):
                    rests.add(r+suff)
                    if r.endswith(suff):rests.add(r[:-len(suff)])
                for mt in re.finditer(r'\d+',r):
                    for i in range(11):rests.add(r[:mt.start()]+str(i).zfill(len(mt[0]))+r[mt.end():])
                if r.endswith(('_f','_y','_n')):
                    rests.update(r[:-1]+t for t in ('f','y','n'))
                for first in ([pp] if code=='shared' else initials):
                    for rest in rests:
                        v=first+'_'+c+'_'+rest
                        if v not in self.names:candidates.add(v);traces[v].add(n)
            hits=self.search('companions',code,targets,[candidates])
            for hit in hits:
                if hit['decoy']==0:
                    hit['seeds']=sorted(traces[hit['name']]);print('COMPANION',hit,flush=True)
                    with (HERE/'companions_evidence.jsonl').open('a',encoding='utf-8') as f:f.write(json.dumps(hit)+'\n')

    def grammar(self):
        seeds=self.supported_seeds()
        suffixes={s for s,count in self.suffix.items() if count>=3 and len(s)<=16}
        suffixes.update(('', '_c','_f','_y','_n','_l','_r','_s','_start','_end','_loop','_po1','_po2','_po3','_d1','_d2'))
        for code in ['shared','inu']+sorted(self.codes-{'inu'}):
            targets=[h for h,r in self.byhash.items() if (len(r['codes'].split())>12 if code=='shared' else r['codes']==code)]
            if not targets:continue
            pc=co.defaultdict(set)
            for n in seeds:
                p=n.split('_',2)
                if len(p)!=3:continue
                if (p[0] in self.shared if code=='shared' else p[0].startswith(code)):
                    pc[p[1]].add(p[0]+'_'+p[1]+'_')
            for cat,prefixes in sorted(pc.items()):
                if cat not in {'at','co','un','dm','th','gd','ro','ra','sta','win'}:continue
                rests=set(self.rests[cat])
                # Learn single-token substitutions from each category's token positions.
                tokens=co.defaultdict(set)
                for r in rests:
                    pp=r.split('_')
                    for i,t in enumerate(pp):tokens[(len(pp),i)].add(t)
                extra=set()
                for r in rests:
                    pp=r.split('_')
                    if len(pp)<2 or len(pp)>4:continue
                    for i in range(len(pp)):
                        for t in tokens[(len(pp),i)]:
                            q=pp.copy();q[i]=t;v='_'.join(q)
                            if len(v)<=40:extra.add(v)
                # Keep separate searches: suffix grafting and positional token PCFG.
                h1=self.search('grammar',code+'_'+cat+'_suffix',targets,[prefixes,rests,suffixes])
                h2=self.search('grammar',code+'_'+cat+'_tokens',targets,[prefixes,extra-rests])
                for hit in h1+h2:
                    if hit['decoy']==0:print('GRAMMAR',hit,flush=True)
            print('grammar complete',code,flush=True)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--cc',default=_P.CC)
    ap.add_argument('--stage',choices=['preflight','targeted','families','companions','grammar'],default='preflight');a=ap.parse_args()
    r=Recovery(a.cc)
    for p in HERE.glob(a.stage+'_*.jsonl'):p.unlink()
    getattr(r,a.stage)()

if __name__=='__main__':main()
