"""Move/neighbor evidence under one budget, with no target-name input.

Training-only first-anchor neighbors are capped at four slots. A finite
evidence lane uses at most 1/4 of the prefix budget; unused slots return to the
unchanged six-gram search. All prefixes still share the same five hash targets.
"""
import t8names_paths as _P
import collections as co
import hashlib
import json
import math
from open_heads import OpenHeadScorer,open_plan
from ranked_prefixes import PrefixPlan,INF
from grammar_benchmark import signature
from recover import body,variants


class NeighborIndex:
    def __init__(self,mappings):
        self.positions={};self.anchors=co.defaultdict(list)
        for row in mappings:
            if 'move_id' not in row:continue
            key=(row['code'],int(row['move_id']))
            if key in self.positions:assert self.positions[key]==row['anim_name']
            self.positions[key]=row['anim_name'];self.anchors[row['anim_name']].append(key)
    def context(self,name,training_names):
        anchors=self.anchors.get(name,())
        if not anchors:return dict(neighbors=[],anchor=None,slots=[])
        code,move_id=min(anchors);slots=[];neighbors=[]
        for delta in (-2,-1,1,2):
            candidate=self.positions.get((code,move_id+delta))
            allowed=candidate in training_names and candidate!=name
            slots.append(dict(offset=delta,visible_name=candidate if allowed else None,
                              state='visible' if allowed else 'missing' if candidate is None else 'masked'))
            if allowed:neighbors.append(candidate)
        return dict(neighbors=neighbors,anchor=[code,move_id],slots=slots)


def shuffled_contexts(split,names,contexts,neighbor_contexts):
    groups=co.defaultdict(list);out={};changed={}
    for name in names:
        row=neighbor_contexts[name];anchor=row['anchor'];code=anchor[0] if anchor else ''
        key=(code,len(contexts[name]['codes'])>=13,len(row['neighbors']))
        groups[key].append(name)
    for key,group in groups.items():
        group.sort(key=lambda n:hashlib.sha256(('context-shuffle-v1:'+split+':'+n).encode()).digest())
        for i,name in enumerate(group):
            source=group[(i+1)%len(group)];out[name]=list(neighbor_contexts[source]['neighbors'])
            changed[name]=source!=name and sorted(out[name])!=sorted(neighbor_contexts[name]['neighbors'])
    return out,changed


class EvidenceScorer:
    def __init__(self,model):
        self.model=model;self.base=OpenHeadScorer(model)
        self.ini_prior=dict((i,p) for p,i in self.base.initials)
        self.cat=co.defaultdict(co.Counter);self.fine=co.defaultdict(co.Counter);seen=set();cat_seen=set()
        for code,move,name in sorted(model.role_samples):
            first,cat,rest=name.split('_',2)
            if len(first)!=5 or first[:3]!=code:continue
            initial=first[3:];sig=signature(move)
            if name not in cat_seen:self.cat[cat][initial]+=1;cat_seen.add(name)
            if (sig,name) not in seen:self.fine[cat,sig][initial]+=1;seen.add((sig,name))

    def scores(self,codes,moves,length,neighbors=()):
        weighted=self.base(self.model,codes,moves,length);sigs=sorted({signature(m) for m in moves if m})
        # Deduplicate repeated neighbors before confidence weighting. Offsets
        # and anchor identity are audit metadata, never model features.
        parsed=[n.split('_',2) for n in sorted(set(neighbors)) if n in self.model.names and len(n.split('_',2))==3]
        firstvotes=co.Counter(p[0] for p in parsed);catvotes=co.Counter(p[1] for p in parsed)
        initialvotes=co.Counter(p[0][3:] for p in parsed if len(p[0])==5)
        factors={}
        for cat in self.model.cats:
            cc=self.cat[cat];cn=sum(cc.values())
            observed=co.Counter();backoff=0
            for sig in sigs:
                fc=self.fine[cat,sig];den=sum(fc.values())+20
                backoff+=20/den/len(sigs)
                for initial,count in fc.items():observed[initial]+=count/den/len(sigs)
            for initial,p0 in self.ini_prior.items():
                cp=(cc[initial]+20*p0)/(cn+20);p=observed[initial]+backoff*cp
                factors[cat,initial]=max(.25,min(4,p/p0)) if sigs else 1
        result=[]
        for weight,head in weighted:
            first,cat,_=head.split('_',2);factor=1
            if len(first)==5 and first[:3] in codes:factor=factors.get((cat,first[3:]),1)
            if parsed:
                factor*=1+firstvotes[first]/len(parsed)+.5*initialvotes[first[3:]]/len(parsed)+.25*catvotes[cat]/len(parsed)
            result.append((weight*factor,head))
        return sorted(result,key=lambda x:(-x[0],x[1]))


def evidence_roots(model,moves,neighbors=()):
    """Weighted complete remainder hypotheses, with auditable origins."""
    weights=co.Counter();origins=co.defaultdict(set);moves=sorted(set(filter(None,moves)))
    def add(word,weight,source):
        if not word or not word.isascii() or set(word)&{'^','$','?',' ','\t','\n'}:return
        if len(word)>24:return
        weights[word]+=weight;origins[word].add(source)
    for move in moves:
        a=body(move);scale=1/max(1,len(moves));literal={a,a.lower()}
        for word in sorted(literal):add(word,4*scale,'move_literal')
        for word in sorted(variants(move)-literal):add(word,scale,'move_variant')
        copied=model.bybody.get(a.lower(),())
        for word in sorted(copied):add(word,8*scale/len(copied),'move_mapping')
        for (start,end,left,right),examples in sorted(model.rules.items()):
            value=a.lower()
            if value.startswith(start) and (not end or value.endswith(end)):
                middle=value[len(start):len(value)-len(end) if end else len(value)]
                if len(middle)>=2:add(left+middle+right,scale*min(2,math.log2(len(examples)+1)/2),'move_rule')
    for neighbor in sorted(set(neighbors)):
        if neighbor not in model.names:continue
        p=neighbor.split('_',2)
        if len(p)!=3:continue
        add(p[2],1,'neighbor_copy')
        for suffix in sorted(model.tails-{''}):
            if p[2].endswith(suffix):add(p[2][:-len(suffix)],.25,'neighbor_stem')
    base=list(weights.items())
    for word,weight in base:
        for suffix in sorted(model.tails-{''}):add(word+suffix,.1*weight/max(1,len(model.tails)-1),'learned_suffix')
    return weights,{word:sorted(v) for word,v in origins.items()}


class GuidedPlan:
    def __init__(self,base,model,moves,neighbors,kind,max_guides=4096,max_heads=256,max_roots_per_length=64):
        self.base=base;self.model_id=kind;self.language=base.language;self.length=base.length
        weights,origins=evidence_roots(model,moves,neighbors);roots=co.defaultdict(list)
        for word,weight in weights.items():
            if len(word)<4 or any(c not in base.alphabet for c in word):continue
            if self.language.prefix_cost(word[:-4]) is None:continue
            roots[len(word)].append((weight,word))
        for n in roots:
            roots[n].sort(key=lambda x:(-x[0],x[1]));roots[n]=roots[n][:max_roots_per_length]
        ranked_heads=[]
        for head,info in base.heads.items():
            if info['rest_length']<4:continue
            cost=info['offset']+self.language.normalizer(info['rest_length'])
            ranked_heads.append((cost,head,info))
        ranked_heads.sort(key=lambda x:(x[0],x[1]));proposals=[]
        for cost,head,info in ranked_heads[:max_heads]:
            rr=roots.get(info['rest_length'],());total=sum(w for w,word in rr)
            for weight,word in rr:
                score=cost+self.language.quantize(-math.log2(weight/total))
                proposals.append((score,head,word[:-4],head+word[:-4],word))
        proposals.sort();items=[];seeds=co.defaultdict(set);used=set()
        for score,head,prefix,full,word in proposals:
            if full in used:seeds[full].add(word);continue
            if len(items)>=max_guides:continue
            used.add(full);seeds[full].add(word);items.append((score,head,prefix,full))
        self.guides=items;self.guide_index={x[3]:i+1 for i,x in enumerate(items)}
        self.seed_remainders={p:sorted(v) for p,v in seeds.items() if p in self.guide_index}
        self.seed_origins=origins
        self.selection_key=hashlib.sha256(json.dumps([items,self.seed_remainders],sort_keys=True).encode()).hexdigest()
        self.context_metadata=dict(guided_prefixes=len(items),root_hypotheses=len(weights),head_limit=max_heads,
                                   roots_per_length_limit=max_roots_per_length,guide_limit=max_guides,allocation='min(guides,floor(budget/4)); remainder to grammar')

    def __getattr__(self,key):return getattr(self.base,key)
    def allocation(self,budget):
        guided=min(len(self.guides),budget//4);return guided,budget-guided
    def cutoff(self,budget):
        g,f=self.allocation(budget);a=self.base.cutoff(f)
        return dict(a,requested=budget,selected=g+a['selected'],guided_selected=g,fallback_budget=f,
                    fallback_selected=a['selected'],global_budget=budget,context=self.context_metadata)
    def truth_bounds(self,name):
        b=self.base.truth_bounds(name)
        if not b['prefix_supported']:return b
        def invert(rank):
            lo=0;hi=2*self.language.hist_cap
            while lo<hi:
                mid=(lo+hi)//2
                if self.allocation(mid)[1]>=rank:hi=mid
                else:lo=mid+1
            return min(self.language.hist_cap,lo)
        guide=self.guide_index.get(name[:-4]);gcost=4*guide if guide is not None else self.language.hist_cap
        return dict(b,rank_lower=min(invert(b['rank_lower']),gcost,self.language.hist_cap),
                    rank_upper=min(invert(b['rank_upper']),gcost,self.language.hist_cap),
                    rank_censored=min(invert(b['rank_upper']),gcost)>=self.language.hist_cap,
                    guided_prefix_rank=guide,grammar_fallback_rank=b['rank_upper'])
    def select_controlled(self,budget,workers,cc):
        g,f=self.allocation(budget)
        if workers>1:
            from parallel_prefixes import select_parallel
            items,a=select_parallel(self.base,f,workers,cc)
        else:items,a=self.base.select_native(f,cc)
        selected=self.guides[:g]+items
        accounting=dict(a,requested=budget,selected=len(selected),guided_selected=g,fallback_budget=f,
            fallback_selected=len(items),global_budget=budget,distinct_prefixes=len({i[3] for i in selected}),
            enumerator='evidence_lane_'+a.get('enumerator','native'),context=self.context_metadata)
        assert len(selected)<=budget
        return selected,accounting
    def select_native(self,budget,cc=_P.CC):return self.select_controlled(budget,1,cc)


def context_plan(model,language,codes,moves,length,neighbors,kind):
    if kind=='open6':return open_plan(model,language,codes,moves,length,kind)
    if not hasattr(model,'_evidence_scorer'):model._evidence_scorer=EvidenceScorer(model)
    use_neighbors=kind in ('context_guided','shuffled_context')
    visible=neighbors if use_neighbors else ()
    scorer=lambda m,c,ms,n:model._evidence_scorer.scores(c,ms,n,visible)
    base=PrefixPlan(model,language,codes,moves,length,scorer);base.model_id=kind
    if kind=='move_heads':return base
    return GuidedPlan(base,model,moves,visible,kind)
