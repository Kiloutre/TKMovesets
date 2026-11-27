"""Plan or execute a bounded character-grammar search with exact tail solving.

Plan first by default. No result is promoted to an accepted animation name.
Every executed search includes the four existing XOR controls and applies the
same grammar predicate to real and decoy outputs. A known target's exact name
is always removed from training; --holdout-family removes its family as well.
"""
import t8names_paths as _P
import argparse
import collections as co
import csv
import json
import math
import subprocess
import time
from tail_solver import HERE,digest
from grammar_benchmark import Model,family,parts
from char_grammar import CharacterGrammar
from recover import read,MASKS

def prepare(target=None,order=3,include_recovered=False,holdout_family=False):
    known=read('known_anim_names.csv');mappings=read('known_move_to_anim.csv')
    names={x['anim_name'] for x in known};truth={x['anim_name'] for x in known if int(x['hash'],16)==target}
    if include_recovered:
        with (HERE.parent/'accepted.csv').open(encoding='utf-8',newline='') as f:accepted=list(csv.DictReader(f))
        for row in accepted:
            names.add(row['name'])
            if int(row['hash'],16)==target:truth.add(row['name'])
            for code in row['codes'].split():
                for move in row['move_names'].split(' | '):mappings.append(dict(code=code,move_name=move,anim_name=row['name']))
    excluded={n for n in names if family(n) in {family(t) for t in truth}} if holdout_family else truth
    model=Model(names-excluded,mappings);language=CharacterGrammar(model.rest,order)
    context=dict(codes=set(),moves=set(),neighbors=[])
    for row in mappings:
        if row['anim_name'] in truth:
            context['codes'].add(row['code']);context['moves'].add(row['move_name'])
    for row in read('unresolved_anim_hashes.csv'):
        if int(row['anim_hash'],16)==target:
            context['codes'].update(row['codes'].split());context['moves'].update(filter(None,row['move_names'].split(' | ')))
            context['placeholder_length']=len(row['placeholder']) or None
            context['neighbors']=[n for n in row['neighbour_anims(id-2..id+2)'].split(' | ')[:4] if n in model.names]
    if truth:
        from context_candidates import NeighborIndex
        index=NeighborIndex(mappings)
        anchored=[index.context(name,model.names) for name in sorted(truth)]
        anchored=[row for row in anchored if row['anchor'] is not None]
        if anchored:
            neighbor_context=min(anchored,key=lambda row:tuple(row['anchor']))
            context['neighbors']=neighbor_context['neighbors']
            context['neighbor_audit']=neighbor_context
    return model,language,context,excluded,truth

def make_plan(model,language,codes,moves,length,heads_mode='context',category_mode='all',head_budget=None):
    heads=model.first if heads_mode=='global' else model.heads(codes)
    pairs={(f,c) for f in heads for c in model.pc.get(f,())}
    if head_budget is not None:
        pairs=set(model.ranked_heads(codes,moves,length)[:head_budget]);heads_mode='ranked'
    if category_mode=='top3':
        _,cats=model.move_roots(sorted(moves));pairs={(f,c) for f,c in pairs if c in cats}
    heads=sorted(f+'_'+c+'_' for f,c in pairs if language.count(length-len(f)-len(c)-2)>0)
    full=sum(language.count(length-len(h)) for h in heads)
    prefix_bound=sum(language.prefix_count(length-len(h)) for h in heads)
    alphabet=''.join(sorted(set(language.alphabet)|set(''.join(heads))))
    overlap=len(alphabet) if length<15 else 1
    plan=dict(length=length,heads_mode=heads_mode,category_mode=category_mode,head_count=len(heads),
              head_budget=head_budget,
              full_grammar_candidates=full,full_grammar_bits=math.log2(full) if full else 0,
              inverse_prefix_upper_bound=prefix_bound,overlap_enumerations=overlap,
              native_word_solves_upper_bound=5*prefix_bound*overlap,
              expected_chance_per_target=full/2**32,alphabet_size=len(alphabet),order=language.order,
              four_decoy_masks=[f'{x:08X}' for x in MASKS[1:]],
              limitation='Length and grammar membership are hypotheses; hash matches remain unreviewed candidates.')
    return plan,heads,alphabet

def execute(target,plan,heads,alphabet,language,cc,prefix_budget):
    if not 13<=plan['length']<=24:raise ValueError('Exact-tail execution supports length 13..24 only')
    if plan['inverse_prefix_upper_bound']>prefix_budget:
        raise ValueError(f"Plan needs at most {plan['inverse_prefix_upper_bound']:,} prefixes, exceeding budget {prefix_budget:,}")
    runid=f'{target:08X}_n{plan["length"]}_o{language.order}_{plan["heads_mode"]}{plan.get("head_budget") or ""}_{plan["category_mode"]}'
    outdir=HERE/'method_jobs'/runid;outdir.mkdir(parents=True,exist_ok=True)
    started=time.perf_counter();prefixes=set();n=plan['length']
    for head in heads:
        for prefix in language.prefixes(n-len(head)):
            prefixes.add((head+prefix)[:n-4])
    assert len(prefixes)<=plan['inverse_prefix_upper_bound']
    pf=outdir/'prefixes.txt';pf.write_text(''.join(x+'\n' for x in sorted(prefixes)),encoding='ascii')
    tf=outdir/'targets.txt';targetmap={target^mask:i for i,mask in enumerate(MASKS)}
    tf.write_text(''.join(f'{h:08X} 0\n' for h in targetmap),encoding='ascii')
    generation=time.perf_counter()-started
    exe=HERE/'tail_solver.exe'
    source_time=max((_P.SRC/'tail_solver.c').stat().st_mtime,(_P.SRC/'kamui_search.c').stat().st_mtime)
    if not exe.exists() or exe.stat().st_mtime<source_time:
        subprocess.run([cc,'-O3','-o',str(exe),str(_P.SRC/'tail_solver.c')],check=True)
    started=time.perf_counter()
    process=subprocess.run([str(exe),str(tf),str(pf),alphabet],capture_output=True,text=True,check=True)
    wall=time.perf_counter()-started;counts=[0]*5;rows=[];allowed=set(heads)
    for line in process.stdout.splitlines():
        hh,name=line.split('\t');h=int(hh,16);assert h in targetmap and digest(name)==h
        p=parts(name)
        if not p or p[0]+'_'+p[1]+'_' not in allowed or not language.accepts(p[2]):continue
        decoy=targetmap[h];counts[decoy]+=1
        rows.append(dict(hash=f'0x{h:08X}',name=name,decoy=decoy,status='unreviewed'))
    with (outdir/'candidates.csv').open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,['hash','name','decoy','status']);w.writeheader();w.writerows(rows)
    (outdir/'raw_hits.txt').write_text(process.stdout,encoding='ascii')
    result=dict(**plan,target=f'0x{target:08X}',prefixes_enumerated=len(prefixes),
                generation_seconds=generation,engine_wall_seconds=wall,native=json.loads(process.stderr),
                real_hits=counts[0],decoy_hits=counts[1:],real_minus_decoy=counts[0]-sum(counts[1:])/4,
                output_directory=str(outdir))
    (outdir/'results.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result,rows

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--hash',required=True,type=lambda s:int(s,16));ap.add_argument('--length',type=int)
    ap.add_argument('--code',action='append');ap.add_argument('--order',type=int,choices=(2,3),default=3)
    ap.add_argument('--heads',choices=('context','global'),default='context')
    ap.add_argument('--head-budget',type=int,help='Search the top N ranked heads instead of hard-filtering by code.')
    ap.add_argument('--categories',choices=('all','top3'),default='all')
    ap.add_argument('--include-recovered',action='store_true');ap.add_argument('--holdout-family',action='store_true')
    ap.add_argument('--execute',action='store_true');ap.add_argument('--prefix-budget',type=int,default=2_000_000)
    ap.add_argument('--cc',default=_P.CC);a=ap.parse_args()
    model,language,context,excluded,truth=prepare(a.hash,a.order,a.include_recovered,a.holdout_family)
    length=a.length or context.get('placeholder_length')
    if length is None:ap.error('No supplied placeholder length; specify a length hypothesis with --length.')
    if not 13<=length<=24:ap.error('This solver currently supports length 13..24.')
    codes=set(a.code) if a.code else context['codes']
    if not codes and a.heads!='global':ap.error('No code metadata; use --code or --heads global.')
    if a.head_budget is not None and a.head_budget<1:ap.error('--head-budget must be positive')
    plan,heads,alphabet=make_plan(model,language,codes,context['moves'],length,a.heads,a.categories,a.head_budget)
    plan.update(target=f'0x{a.hash:08X}',training_names=len(model.names),excluded_target_names=len(excluded),
                within_prefix_budget=plan['inverse_prefix_upper_bound']<=a.prefix_budget)
    if a.execute:
        # execute() adds target to its result; avoid a duplicate keyword.
        plan.pop('target')
        result,_=execute(a.hash,plan,heads,alphabet,language,a.cc,a.prefix_budget)
        print(json.dumps(result,indent=2))
    else:print(json.dumps(plan,indent=2))

if __name__=='__main__':main()
