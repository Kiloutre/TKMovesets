"""Bounded weighted-prefix searches. Every run includes the four XOR controls.

Both grammar-filtered and alphabet-only tail results are retained separately.
Neither output is an accepted-name list. Without --execute only a budget plan
is computed. Known targets are removed from training automatically.
"""
import t8names_paths as _P
import argparse
import collections as co
import csv
import hashlib
import json
import subprocess
import time
from ranked_prefixes import WeightedGrammar,PrefixPlan,HERE
from search_method import prepare
from recover import MASKS,digest

def represented_counts(plan,prefixes):
    language=plan.language;alphabet_size=len(plan.alphabet);n=plan.length;short=co.defaultdict(lambda:[0,0])
    for head,info in plan.heads.items():
        if info['rest_length']<4:
            pair=short[head[:n-4]];pair[0]+=language.count(info['rest_length']);pair[1]+=alphabet_size**info['rest_length']
    grammar=alphabet=0
    for prefix in prefixes:
        p=prefix.split('_',2)
        if len(p)<3:
            a,b=short[prefix];grammar+=a;alphabet+=b;continue
        head=p[0]+'_'+p[1]+'_'
        assert head in plan.heads
        state=language.start
        for ch in p[2]:
            assert ch in language.transitions[state];state=language.advance(state,ch)
        grammar+=language.completions(4,state);alphabet+=alphabet_size**4
    return dict(grammar=grammar,alphabet=alphabet)

def execute_ranked(target,plan,budget,cc=_P.CC,workers=1):
    if not isinstance(workers,int) or isinstance(workers,bool) or not 1<=workers<=32:raise ValueError('workers must be1..32')
    started=time.perf_counter()
    if hasattr(plan,'select_controlled'):items,accounting=plan.select_controlled(budget,workers,cc)
    elif workers>1:
        from parallel_prefixes import select_parallel
        items,accounting=select_parallel(plan,budget,workers,cc)
    else:items,accounting=plan.select_native(budget,cc)
    prefix_set={x[3] for x in items};prefixes=sorted(prefix_set);space=represented_counts(plan,prefixes)
    identity=[plan.heads,plan.language.hist_key]
    if hasattr(plan,'selection_key'):identity.append(plan.selection_key)
    signature=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:10]
    label=getattr(plan,'model_id',getattr(plan.language,'model_id','a'+str(plan.language.backoff)))
    outdir=HERE/'ranked_runs'/f'{target:08X}_n{plan.length}_{label}_b{budget}_{signature}'
    outdir.mkdir(parents=True,exist_ok=True)
    pf=outdir/'prefixes.txt';pf.write_text(''.join(p+'\n' for p in prefixes),encoding='ascii')
    tf=outdir/'targets.txt';targetmap={target^m:i for i,m in enumerate(MASKS)}
    tf.write_text(''.join(f'{h:08X} 0\n' for h in targetmap),encoding='ascii')
    generation=time.perf_counter()-started
    from native_cache import compiled
    exe=compiled(_P.SRC/'tail_solver.c',HERE/'rank_cache',cc,dependencies=(_P.SRC/'kamui_search.c',))
    started=time.perf_counter()
    run=subprocess.run([str(exe),str(tf),str(pf),plan.alphabet],capture_output=True,text=True,check=True)
    elapsed=time.perf_counter()-started;counts={mode:[0]*5 for mode in ('grammar','alphabet')};rows=[]
    for line in run.stdout.splitlines():
        hh,name=line.split('\t');h=int(hh,16)
        assert digest(name)==h and h in targetmap and name[:-4] in prefix_set
        p=name.split('_',2)
        if len(p)!=3 or p[0]+'_'+p[1]+'_' not in plan.heads:continue
        decoy=targetmap[h];grammar=plan.language.accepts(p[2]);counts['alphabet'][decoy]+=1
        if grammar:counts['grammar'][decoy]+=1
        rows.append(dict(hash=f'0x{h:08X}',name=name,decoy=decoy,grammar_matches=int(grammar),status='unreviewed'))
    for mode in ('alphabet','grammar'):
        with (outdir/(mode+'_candidates.csv')).open('w',encoding='utf-8',newline='') as f:
            w=csv.DictWriter(f,['hash','name','decoy','grammar_matches','status']);w.writeheader()
            w.writerows(x for x in rows if mode=='alphabet' or x['grammar_matches'])
    (outdir/'raw_hits.txt').write_text(run.stdout,encoding='ascii')
    results={mode:dict(candidates=space[mode],expected_chance_per_target=space[mode]/2**32,
                      real_hits=counts[mode][0],decoys=counts[mode][1:],decoy_mean=sum(counts[mode][1:])/4,
                      real_minus_decoy=counts[mode][0]-sum(counts[mode][1:])/4) for mode in counts}
    result=dict(target=f'0x{target:08X}',length=plan.length,backoff=plan.language.backoff,model=label,head_count=len(plan.heads),
                budget=accounting,prefixes=len(prefixes),prefix_sha256=hashlib.sha256(pf.read_bytes()).hexdigest(),
                generation_seconds=generation,engine_wall_seconds=elapsed,native=json.loads(run.stderr),
                results=results,output_directory=str(outdir),interpretation='All hits are unreviewed; alphabet-only tails widen coverage and increase collisions.')
    if hasattr(plan,'seed_remainders'):
        evidence=[]
        for row in rows:
            remainder=row['name'].split('_',2)[2];prefix=row['name'][:-4]
            if prefix not in plan.seed_remainders:continue
            evidence.append(dict(hash=row['hash'],name=row['name'],decoy=row['decoy'],
                full_remainder_matches_seed=remainder in plan.seed_remainders[prefix],
                origins=plan.seed_origins.get(remainder,[]) if remainder in plan.seed_remainders[prefix] else [],status='unreviewed'))
        (outdir/'context_evidence.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')
        result['context']=plan.context_metadata
    (outdir/'results.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result,rows

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--hash',required=True,type=lambda s:int(s,16));ap.add_argument('--length',type=int)
    ap.add_argument('--code',action='append');ap.add_argument('--prefix-budget',type=int,default=10000)
    ap.add_argument('--backoff',type=float,default=.1);ap.add_argument('--include-recovered',action='store_true')
    ap.add_argument('--model',choices=['char3','token_trie','subword6','subword6_pooled','adaptive6','adaptive6_pooled','open6','open9','move_heads','move_guided','context_guided'],default='char3',help='Training-only ranking model; open and context models also generate unseen heads.')
    ap.add_argument('--holdout-family',action='store_true');ap.add_argument('--execute',action='store_true')
    ap.add_argument('--workers',type=int,default=1,help='Native prefix workers sharing one global budget (1..32).')
    ap.add_argument('--cc',default=_P.CC);a=ap.parse_args()
    if not 0<=a.hash<=0xffffffff:ap.error('Hash must be an unsigned 32-bit value.')
    if not 1<=a.workers<=32:ap.error('Workers must be 1..32.')
    if not 0<a.prefix_budget<=1_000_000:ap.error('Prefix budget must be 1..1,000,000.')
    if not 0<=a.backoff<=1:ap.error('Backoff must be between 0 and 1.')
    if a.model!='char3' and a.backoff!=.1:ap.error('--backoff only configures char3; long model settings are frozen.')
    model,_,context,excluded,truth=prepare(a.hash,3,a.include_recovered,a.holdout_family)
    length=a.length or context.get('placeholder_length')
    if length is None or not 13<=length<=24:ap.error('Specify a length hypothesis in 13..24 when a supported placeholder length is unavailable.')
    codes=set(a.code) if a.code else context['codes']
    started=time.perf_counter()
    if a.model=='char3':language=WeightedGrammar(model.rest,a.backoff)
    elif a.model in ('open6','open9','move_heads','move_guided','context_guided'):
        from open_heads import open_language
        language=open_language(a.model if a.model in ('open6','open9') else 'open6',model.rest)
    elif a.model in ('subword6_pooled','adaptive6','adaptive6_pooled'):
        from refined_models import refined_language
        language=refined_language(a.model,model.rest)
    else:
        from long_models import make_language
        language=make_language(a.model,model.rest)
    language.build_histograms(cc=a.cc)
    if a.model in ('move_heads','move_guided','context_guided'):
        from context_candidates import context_plan
        plan=context_plan(model,language,codes,context['moves'],length,context['neighbors'],a.model)
    elif a.model in ('open6','open9'):
        from open_heads import open_plan
        plan=open_plan(model,language,codes,context['moves'],length,a.model)
    elif a.model in ('subword6_pooled','adaptive6','adaptive6_pooled'):
        from refined_models import refined_plan
        plan=refined_plan(model,language,codes,context['moves'],length,a.model)
    else:plan=PrefixPlan(model,language,codes,context['moves'],length)
    if a.execute:result,_=execute_ranked(a.hash,plan,a.prefix_budget,a.cc,workers=a.workers)
    else:
        accounting=plan.cutoff(a.prefix_budget)
        result=dict(target=f'0x{a.hash:08X}',length=length,backoff=a.backoff,head_count=len(plan.heads),budget=accounting,
                    native_word_solves_upper_bound=5*accounting['selected']*(len(plan.alphabet) if length<15 else 1),
                    setup_seconds=time.perf_counter()-started,interpretation='A bounded search plan; grammar and prefix budgets can exclude the true name.')
    result.update(model=a.model,training_names=len(model.names),excluded_target_names=len(excluded))
    if a.model in ('move_heads','move_guided','context_guided'):
        result['input_neighbors']=context['neighbors']
        result['context_metadata']=getattr(plan,'context_metadata',{})
    print(json.dumps(result,indent=2))

if __name__=='__main__':main()
