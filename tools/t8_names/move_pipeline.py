"""Independent move-label grammar and candidate priors; no animation parser.

The only target features are known owner codes and total byte length. Full
target names are used by evaluation only, never to construct a candidate plan.
"""
import collections as co
import hashlib
import json
import re
from pathlib import Path
from long_models import SubwordGrammar
from tail_solver import HERE
from typed_search import GenericPrefixPlan

VARIANTS=('pooled_t8','move_t8','move_t8_t7')
LABEL_FIELD={'move':'move_name','animation':'anim_name'}
TERMINAL_VARIANTS=frozenset(('f','y','n','c','l','r','e','end','start','loop','cancel','ex','cs','sp'))


def sha_names(names):
    return hashlib.sha256(('\n'.join(sorted(set(names)))+'\n').encode('utf-8')).hexdigest()


def split_move(name):
    """First underscore only. Case and the entire command body are preserved."""
    if '_' not in name:return None
    head,body=name.split('_',1)
    if not head or not body:return None
    return head+'_',body


def move_family(name):
    """Conservative hidden-family key: ignore owner, digits, terminal variants."""
    pair=split_move(name);body=(pair[1] if pair else name).lower()
    body=re.sub(r'\d+','',body)
    parts=[p for p in body.split('_') if p]
    while parts and parts[-1] in TERMINAL_VARIANTS:parts.pop()
    return '_'.join(parts) or '<empty>'


def hidden_family(family,fold):
    return int.from_bytes(hashlib.sha256((fold+':'+family).encode()).digest()[:8],'big')%5==0


def native_name(name):
    return bool(name) and name.isascii() and all(33<=ord(c)<=126 and c not in '^$?' for c in name)


class MoveModel:
    def __init__(self,rows,names,model_id):
        self.model_id=model_id;self.names=set(names);self.head_global=co.Counter();self.head_code=co.defaultdict(co.Counter)
        self.head_lengths=co.defaultdict(co.Counter);self.bare_global=co.Counter();self.bare_code=co.defaultdict(co.Counter)
        self.bodies=set();self.rejected=[];seen=set();global_seen=set()
        for row in rows:
            name=row['label'];code=row['code']
            if name not in self.names or (code,name) in seen:continue
            seen.add((code,name))
            if not native_name(name):self.rejected.append(name);continue
            pair=split_move(name)
            if pair:
                head,body=pair;self.head_code[code][head]+=1;self.bodies.add(body)
                if name not in global_seen:self.head_global[head]+=1;self.head_lengths[head][len(name)]+=1
            else:
                self.bare_code[code][name]+=1
                if name not in global_seen:self.bare_global[name]+=1
            global_seen.add(name)
        assert self.bodies and self.head_global
        self.details=dict(model=model_id,training_names=len(self.names),body_count=len(self.bodies),heads=len(self.head_global),
            bare_names=len(self.bare_global),native_rejected_names=len(set(self.rejected)),training_sha256=sha_names(self.names),
            body_sha256=sha_names(self.bodies),head_source='Training-only code owners, with global backoff; no target-name feature')

    def weighted_heads(self,codes,length):
        total=sum(self.head_global.values());base=co.Counter({h:.05*n/total for h,n in self.head_global.items()})
        codes=sorted(set(codes));local=[self.head_code[c] for c in codes if self.head_code[c]]
        if local:
            for counter in local:
                den=sum(counter.values())
                for h,n in counter.items():base[h]+=.95*n/den/len(local)
        else:
            for h,n in self.head_global.items():base[h]+=.95*n/total
        weighted=[]
        for head,n in sorted(self.head_global.items()):
            if len(head)>=length:continue
            counts=self.head_lengths[head]
            likelihood=(counts[length]+.5*(counts[length-1]+counts[length+1])+.02)/(2*n+.5)
            weighted.append((base[head]*likelihood,head))
        return sorted(weighted,key=lambda x:(-x[0],x[1]))

    def finite_candidates(self,codes=()):
        votes=co.Counter(self.bare_global)
        for code in sorted(set(codes)):
            for name,n in self.bare_code[code].items():votes[name]+=20*n
        return [name for name,n in sorted(votes.items(),key=lambda x:(-x[1],x[0]))]

    def language(self):return SubwordGrammar(self.bodies,max_history=5,high_weight=.75,quantum=8)

    def plan(self,language,codes,length):
        return GenericPrefixPlan(language,self.weighted_heads(codes,length),length,namespace='move',model_id=self.model_id)


def source_names(corpus,namespace,t7_policy='none'):
    return corpus.training_view(namespace=namespace,t7_policy=t7_policy,t7_overlap='mask')


def build_training(corpus,fold,variant):
    if variant not in VARIANTS:raise ValueError(variant)
    original=source_names(corpus,'move');hidden={n for n in original.names if hidden_family(move_family(n),fold)}
    families={move_family(n) for n in hidden}
    masks=dict(hidden_names={'move':hidden,'animation':hidden},hidden_families={'move':families,'animation':families},
        family_key={'move':move_family,'animation':move_family},t7_overlap='mask')
    policy='unflagged' if variant=='move_t8_t7' else 'none'
    selected=[('move',corpus.training_view(namespace='move',t7_policy=policy,**masks))]
    if variant=='pooled_t8':selected.append(('animation',corpus.training_view(namespace='animation',t7_policy='none',**masks)))
    names=set();rows=[];stats={}
    for namespace,view in selected:
        names.update(view.names);stats[namespace]=view.stats
        for row in view.rows:
            name=row[LABEL_FIELD[namespace]]
            if name:rows.append(dict(label=name,code=row['code'],game=row['game'],source_id=row['source_id'],namespace=namespace))
    assert not names&hidden
    assert not {move_family(n) for n in names}&families
    model=MoveModel(rows,names,variant)
    provenance=sorted({(r['game'],r['source_id'],r['namespace']) for r in rows})
    model.details.update(fold=fold,hidden_names=len(hidden),hidden_families=len(families),hidden_sha256=sha_names(hidden),
        hidden_family_sha256=sha_names(families),strict_family_disjoint=True,source_stats=stats,source_provenance=provenance,
        t7_policy=policy,t7_overlap='mask',t7_code_policy='Raw T7 owners contribute global heads; no invented cross-game code mapping')
    return model,hidden


def freeze_cases(corpus,config):
    """Outcome-independent selection; each fold samples short and long strata."""
    view=source_names(corpus,'move');owners=co.defaultdict(set)
    for row in view.rows:
        if row['move_name']:owners[row['move_name']].add(row['code'])
    t7=source_names(corpus,'move',t7_policy='unflagged').t7_names
    t7bodies={split_move(n)[1] if split_move(n) else n for n in t7};t7families={move_family(n) for n in t7}
    cases=[];seen=set();folds=[]
    for fold in config['folds']:
        hidden={n for n in view.names if hidden_family(move_family(n),fold)};eligible={n for n in hidden if 13<=len(n)<=24 and native_name(n)}
        selected=[];strata=[]
        for lo,hi,wanted in config['sample_strata']:
            pool=sorted((n for n in eligible-seen if lo<=len(n)<=hi),key=lambda n:(hashlib.sha256((config['sample_salt']+':'+fold+':'+n).encode()).hexdigest(),n))
            chosen=pool[:wanted];selected.extend(chosen);strata.append(dict(min_length=lo,max_length=hi,eligible=len(pool),selected=len(chosen)))
        for n in selected:
            pair=split_move(n);body=pair[1] if pair else n
            cases.append(dict(fold=fold,name=n,length=len(n),codes=sorted(owners[n]),family=move_family(n),
                t7_exact_available=n in t7,t7_body_available=body in t7bodies,t7_family_available=move_family(n) in t7families,
                lane='prefix' if pair else 'finite',case_id=hashlib.sha256((fold+'\0'+n).encode()).hexdigest()[:16]))
        seen.update(selected);folds.append(dict(fold=fold,eligible=len(eligible),hidden=len(hidden),strata=strata))
    return dict(configuration=config,cases=cases,folds=folds,case_sha256=sha_names(c['fold']+'\0'+c['name'] for c in cases),
        sampling='SHA-selected equal short/long strata; unweighted results describe this sample, not corpus prevalence.',
        source_metadata=corpus.sources)


def load_config():return json.loads((HERE/'move_pipeline_config.json').read_text(encoding='utf-8'))


if __name__=='__main__':
    import argparse
    from name_corpora import load_corpus
    from typed_search import execute_typed_prefix,execute_candidates
    ap=argparse.ArgumentParser();ap.add_argument('--hash',required=True,type=lambda s:int(s,16));ap.add_argument('--length',type=int)
    ap.add_argument('--codes',nargs='*',default=[]);ap.add_argument('--model',choices=VARIANTS,default='move_t8_t7')
    ap.add_argument('--budget',type=int,default=100000);ap.add_argument('--workers',type=int,default=4);ap.add_argument('--finite',action='store_true');ap.add_argument('--execute',action='store_true')
    a=ap.parse_args()
    if not a.finite and a.length is None:ap.error('--length is required for prefix searches')
    corpus=load_corpus()
    known={r['move_name'] for r in corpus.t8_rows if r['move_status']=='verified' and r['move_name'] and r['move_hash'] is not None and int(str(r['move_hash']),16)==a.hash}
    masks=dict(hidden_names={'move':known,'animation':known},family_key=move_family,t7_overlap='mask')
    views=[('move',corpus.training_view('move',t7_policy='unflagged' if a.model=='move_t8_t7' else 'none',**masks))]
    if a.model=='pooled_t8':views.append(('animation',corpus.training_view('animation',t7_policy='none',**masks)))
    rows=[dict(label=r[LABEL_FIELD[ns]],code=r['code'],game=r['game'],source_id=r['source_id']) for ns,v in views for r in v.rows if r[LABEL_FIELD[ns]]]
    model=MoveModel(rows,set().union(*(v.names for ns,v in views)),a.model)
    if a.finite:
        candidates=model.finite_candidates(a.codes)
        if a.execute:result,_=execute_candidates(a.hash,candidates,'move',a.budget,label=a.model)
        else:result=dict(namespace='move',mode='finite_full_names',budget_unit='complete_names',budget=a.budget,available=len(candidates),selected=min(a.budget,len(candidates)),candidate_sha256=hashlib.sha256(('\n'.join(candidates[:a.budget])+'\n').encode('ascii')).hexdigest())
    else:
        language=model.language().build_histograms();plan=model.plan(language,a.codes,a.length)
        if a.execute:result,_=execute_typed_prefix(a.hash,plan,a.budget,a.workers)
        else:result=dict(namespace='move',mode='prefix',budget_unit='prefix_proposals',budget=plan.cutoff(a.budget),training=model.details)
    result.update(executed=a.execute,known_target_labels_masked=sorted(known))
    print(json.dumps(result,indent=2))
