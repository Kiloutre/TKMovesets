"""Type-neutral Kamui engine: distinct name models, identical hash controls.

Prefix budgets are charged proposals. Finite-name budgets are full strings;
those different units are always recorded separately. No automatic acceptance.
"""
import t8names_paths as _P
import collections as co
import csv
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time
from ranked_prefixes import PrefixPlan,HERE,INF
from recover import MASKS,digest
from native_cache import compiled

CC=_P.CC


def _namespace(value):
    if value not in ('move','animation'):raise ValueError('namespace must be move or animation')
    return value


class GenericPrefixPlan(PrefixPlan):
    """A prefix-free head pool and a remainder language, independent of type."""
    def __init__(self,language,weighted_heads,length,namespace='move',model_id='typed'):
        if not 13<=length<=24:raise ValueError('Prefix inversion supports length13..24')
        self.namespace=_namespace(namespace);self.model_id=model_id;self.language=language;self.length=length
        weights=co.Counter()
        for weight,head in weighted_heads:
            if not head or not head.isascii() or len(head)>=128 or any(ord(c)<33 or ord(c)>126 or c in '^$?' for c in head):raise ValueError('Heads must be nonempty printable native tokens')
            if not math.isfinite(weight) or weight<=0:continue
            if language.mass(length-len(head),language.start)>0:weights[head]+=weight
        ordered=sorted(weights)
        if any(b.startswith(a) for a,b in zip(ordered,ordered[1:])):raise ValueError('Head pool must be prefix-free')
        total=sum(weights.values());self.heads={}
        for head,weight in sorted(weights.items()):
            rest=length-len(head);headcost=language.quantize(-math.log2(weight/total))
            self.heads[head]=dict(depth=max(0,rest-4),rest_length=rest,
                offset=headcost-language.normalizer(rest) if rest>=4 else headcost,singleton=rest<4)
        self.alphabet=''.join(sorted(set(language.alphabet)|set(''.join(self.heads))))
        self.count_groups=co.Counter((v['depth'],v['offset'],v['singleton']) for v in self.heads.values())
        self.head_lengths=sorted({len(h) for h in self.heads})

    def decompose(self,name):
        for n in self.head_lengths:
            if name[:n] in self.heads:return name[:n],name[n:]
        return None

    def accepts(self,name,grammar=True):
        if len(name)!=self.length or any(ch not in self.alphabet for ch in name):return False
        pair=self.decompose(name)
        return bool(pair and (not grammar or self.language.accepts(pair[1])))

    def truth_bounds(self,name):
        pair=self.decompose(name)
        if not pair or len(name)!=self.length:return dict(prefix_supported=False,grammar_supported=False)
        head,rest=pair;info=self.heads[head];strict=self.language.accepts(rest)
        if any(c not in self.alphabet for c in name[-4:]):return dict(prefix_supported=False,grammar_supported=False)
        cost=0 if info['singleton'] else self.language.prefix_cost(rest[:-4])
        if cost is None:return dict(prefix_supported=False,grammar_supported=strict)
        score=cost+info['offset']
        if info['singleton']:
            physical=name[:-4];score=min(v['offset'] for h,v in self.heads.items() if v['singleton'] and h[:self.length-4]==physical)
        return dict(prefix_supported=True,grammar_supported=strict,score=score,
            rank_lower=min(self.language.hist_cap,1+self.count_leq(score-1)),rank_upper=self.count_leq(score),
            rank_censored=self.count_leq(score)>=self.language.hist_cap)

    def represented_counts(self,prefixes):
        short=co.defaultdict(lambda:[0,0]);language=self.language;size=len(self.alphabet)
        for head,v in self.heads.items():
            if v['singleton']:
                pair=short[head[:self.length-4]];pair[0]+=language.count(v['rest_length']);pair[1]+=size**v['rest_length']
        grammar=alphabet=0
        for prefix in prefixes:
            pair=self.decompose(prefix)
            if pair:
                head,rest=pair;state=language.start
                for ch in rest:
                    assert ch in language.transitions[state];state=language.advance(state,ch)
                grammar+=language.completions(4,state);alphabet+=size**4
            else:
                assert prefix in short
                a,b=short[prefix];grammar+=a;alphabet+=b
        return dict(grammar=grammar,alphabet=alphabet)


def _accepts(plan,name,grammar=True):
    if hasattr(plan,'accepts'):return plan.accepts(name,grammar)
    p=name.split('_',2)
    return len(name)==plan.length and len(p)==3 and p[0]+'_'+p[1]+'_' in plan.heads and (not grammar or plan.language.accepts(p[2]))


def _represented(plan,prefixes):
    if hasattr(plan,'represented_counts'):return plan.represented_counts(prefixes)
    from search_ranked import represented_counts
    return represented_counts(plan,prefixes)


def execute_typed_prefix(target,plan,budget,workers=4,cc=CC,namespace=None):
    namespace=_namespace(namespace or getattr(plan,'namespace','animation'))
    if not 0<=target<=0xffffffff:raise ValueError('Target must fit32bits')
    if not isinstance(workers,int) or isinstance(workers,bool) or not 1<=workers<=32:raise ValueError('Workers must be1..32')
    started=time.perf_counter()
    if hasattr(plan,'select_controlled'):items,accounting=plan.select_controlled(budget,workers,cc)
    elif workers>1:
        from parallel_prefixes import select_parallel
        items,accounting=select_parallel(plan,budget,workers,cc)
    else:items,accounting=plan.select_native(budget,cc)
    prefixes=sorted({item[3] for item in items});assert len(items)<=budget and len(prefixes)<=len(items)
    space=_represented(plan,prefixes);label=getattr(plan,'model_id','typed')
    identity=[namespace,plan.heads,plan.language.hist_key,getattr(plan,'selection_key','')]
    signature=hashlib.sha256(json.dumps(identity,sort_keys=True).encode()).hexdigest()[:12]
    directory=HERE/'typed_runs'/f'{namespace}_{target:08X}_{label}_n{plan.length}_b{budget}_{signature}';directory.mkdir(parents=True,exist_ok=True)
    pf=directory/'prefixes.txt';pf.write_text(''.join(p+'\n' for p in prefixes),encoding='ascii')
    targets={target^mask:i for i,mask in enumerate(MASKS)}
    tf=directory/'targets.txt';tf.write_text(''.join(f'{h:08X} 0\n' for h in targets),encoding='ascii')
    generation=time.perf_counter()-started
    exe=compiled(_P.SRC/'tail_solver.c',HERE/'rank_cache',cc,dependencies=(_P.SRC/'kamui_search.c',))
    started=time.perf_counter();run=subprocess.run([str(exe),str(tf),str(pf),plan.alphabet],capture_output=True,text=True,check=True)
    elapsed=time.perf_counter()-started;prefixset=set(prefixes);rows=[];counts={mode:[0]*5 for mode in ('grammar','alphabet')}
    for line in run.stdout.splitlines():
        hh,name=line.split('\t');h=int(hh,16)
        assert h in targets and digest(name)==h and name[:-4] in prefixset
        if not _accepts(plan,name,False):continue
        decoy=targets[h];strict=_accepts(plan,name,True)
        counts['alphabet'][decoy]+=1;counts['grammar'][decoy]+=int(strict)
        row=dict(namespace=namespace,hash=f'0x{h:08X}',name=name,decoy=decoy,grammar_matches=int(strict),status='unreviewed')
        rows.append(row)
    for mode in ('grammar','alphabet'):
        with (directory/(mode+'_candidates.csv')).open('w',encoding='utf-8',newline='') as f:
            writer=csv.DictWriter(f,['namespace','hash','name','decoy','grammar_matches','status']);writer.writeheader()
            writer.writerows(r for r in rows if mode=='alphabet' or r['grammar_matches'])
    (directory/'raw_hits.txt').write_text(run.stdout,encoding='ascii')
    result=dict(namespace=namespace,target=f'0x{target:08X}',model=label,length=plan.length,budget_unit='prefix_proposals',budget=accounting,
        prefixes=len(prefixes),prefix_sha256=hashlib.sha256(pf.read_bytes()).hexdigest(),generation_seconds=generation,
        engine_wall_seconds=elapsed,native=json.loads(run.stderr),output_directory=str(directory),
        results={mode:dict(candidates=space[mode],expected_chance_per_target=space[mode]/2**32,real_hits=counter[0],decoys=counter[1:],
            decoy_mean=sum(counter[1:])/4,real_minus_decoy=counter[0]-sum(counter[1:])/4) for mode,counter in counts.items()},
        controls=[f'{m:08X}' for m in MASKS],interpretation='Unreviewed candidates. Prefix-derived evidence does not establish solved-tail correctness.')
    if hasattr(plan,'seed_remainders'):
        evidence=[]
        for r in rows:
            name=r['name'];prefix=name[:-4]
            if prefix not in plan.seed_remainders:continue
            if hasattr(plan,'seed_evidence'):
                evidence.append(dict(r,**plan.seed_evidence(name)))
            else:
                pair=plan.decompose(name) if hasattr(plan,'decompose') else None
                remainder=pair[1] if pair else name.split('_',2)[-1]
                full=remainder in plan.seed_remainders[prefix]
                evidence.append(dict(**r,full_remainder_matches_seed=full,origins=plan.seed_origins.get(remainder,[]) if full else []))
        (directory/'seed_evidence.json').write_text(json.dumps(evidence,indent=2),encoding='utf-8')
        if hasattr(plan,'transfer_provenance'):
            (directory/'transfer_hypotheses.json').write_text(json.dumps(plan.transfer_provenance,indent=2),encoding='utf-8')
    (directory/'results.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result,rows


def execute_candidates(target,candidates,namespace,limit=1_000_000,cc=CC,label='finite'):
    """Finite full names, all lengths; one target or an iterable of real targets."""
    namespace=_namespace(namespace)
    if limit<1:raise ValueError('Name budget must be positive')
    targets=[target] if isinstance(target,int) else sorted(set(target))
    if not targets or any(not 0<=t<=0xffffffff for t in targets):raise ValueError('Targets must fit32bits')
    ordered=[];seen=set()
    for name in candidates:
        if not isinstance(name,str) or not name or not name.isascii() or any(ord(c)<32 or ord(c)>126 for c in name):continue
        if len(name)>509:raise ValueError('Full-name candidate exceeds native input line limit')
        if name in seen:continue
        seen.add(name);ordered.append(name)
        if len(ordered)>=limit:break
    if not ordered:raise ValueError('No valid full-name candidates')
    data='\n'.join(ordered)+'\n';sha=hashlib.sha256(data.encode('ascii')).hexdigest()
    tag=hashlib.sha256(json.dumps(targets).encode()).hexdigest()[:10]
    directory=HERE/'typed_runs'/f'{namespace}_finite_{label}_{tag}_{sha[:12]}';directory.mkdir(parents=True,exist_ok=True)
    nf=directory/'names.txt';nf.write_text(data,encoding='ascii')
    targetmap=co.defaultdict(list)
    for t in targets:
        for i,mask in enumerate(MASKS):targetmap[t^mask].append((t,i))
    if len(targetmap)>=65536:raise ValueError('Too many target/control hashes for native table')
    tf=directory/'targets.txt';tf.write_text(''.join(f'{h:08X} 0\n' for h in sorted(targetmap)),encoding='ascii')
    exe=compiled(_P.SRC/'kamui_search.c',HERE/'rank_cache',cc)
    started=time.perf_counter();run=subprocess.run([str(exe),str(tf),str(nf)],capture_output=True,text=True,check=True);elapsed=time.perf_counter()-started
    counts={t:[0]*5 for t in targets};rows=[];raw=set()
    for line in run.stdout.splitlines():
        hh,name=line.split('\t');h=int(hh,16);assert name in seen and h in targetmap and digest(name)==h
        assert (h,name) not in raw;raw.add((h,name))
        for t,i in targetmap[h]:
            counts[t][i]+=1;rows.append(dict(namespace=namespace,target=f'0x{t:08X}',hash=f'0x{h:08X}',name=name,decoy=i,status='unreviewed'))
    result=dict(namespace=namespace,mode='finite_full_names',budget_unit='complete_names',requested_budget=limit,candidates=len(ordered),
        candidate_sha256=sha,real_targets=len(targets),unique_hash_queries=len(targetmap),engine_wall_seconds=elapsed,output_directory=str(directory),
        expected_chance_per_target=len(ordered)/2**32,controls=[f'{m:08X}' for m in MASKS],
        target_results=[dict(target=f'0x{t:08X}',real_hits=ccount[0],decoys=ccount[1:],decoy_mean=sum(ccount[1:])/4,
            real_minus_decoy=ccount[0]-sum(ccount[1:])/4) for t,ccount in counts.items()],
        interpretation='Finite-name budget is distinct from prefix budget; all hits need independent evidence. Shared target/control hashes can correlate counts.')
    with (directory/'candidates.csv').open('w',newline='',encoding='utf-8') as f:
        writer=csv.DictWriter(f,['namespace','target','hash','name','decoy','status']);writer.writeheader();writer.writerows(rows)
    (directory/'raw_hits.txt').write_text(run.stdout,encoding='ascii');(directory/'results.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    return result,rows
