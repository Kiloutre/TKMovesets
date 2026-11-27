"""Weighted, length-conditioned prefixes with exact quantized-cost budgets.

Costs are integer eighth-bits. A prefix is scored by all possible four-byte
completions, not by its best completion. Native histograms count proposals at
each cost, saturating above the configured budget. Enumeration then visits only
the selected cost region, with bounded result storage and a depth-first stack.
"""
import t8names_paths as _P
import bisect
import collections as co
from functools import lru_cache
import hashlib
import json
import math
from pathlib import Path
import subprocess
from char_grammar import CharacterGrammar
from tail_solver import HERE
from grammar_benchmark import signature
from native_cache import text_once,compiled,run_to_file

INF=10**9

class WeightedGrammar(CharacterGrammar):
    def __init__(self,words,backoff=0.1,quantum=8):
        assert 0<=backoff<=1 and quantum>0
        self.order=3;self.start='^^';self.backoff=backoff;self.quantum=quantum
        tri=co.defaultdict(co.Counter);bi=co.defaultdict(co.Counter);alphabet=set()
        for word in sorted(set(words)):
            assert not set(word)&{'^','$','?'}
            state=self.start;alphabet.update(word)
            for ch in word+'$':
                tri[state][ch]+=1;bi[state[-1]][ch]+=1;state=(state+ch)[-2:]
        self.alphabet=''.join(sorted(alphabet));self.probabilities={};self.transitions={};self.costs={}
        pending=[self.start];seen=set()
        while pending:
            state=pending.pop()
            if state in seen:continue
            seen.add(state);tc=tri[state];bc=bi[state[-1]];tn=sum(tc.values());bn=sum(bc.values())
            chars=set(tc)|(set(bc) if backoff else set())
            probabilities={}
            for ch in sorted(chars):
                if not backoff:p=tc[ch]/tn if tn else 0
                elif not tn:p=bc[ch]/bn if bn else 0
                else:p=(1-backoff)*tc[ch]/tn+backoff*(bc[ch]/bn if bn else 0)
                if p:probabilities[ch]=p
            self.probabilities[state]=probabilities;self.transitions[state]=tuple(probabilities)
            self.costs[state]={ch:self.quantize(-math.log2(p)) for ch,p in probabilities.items()}
            pending.extend((state+ch)[-2:] for ch in probabilities if ch!='$')
        self._hist=None
    def quantize(self,bits):return int(math.floor(bits*self.quantum+.5))
    @lru_cache(maxsize=None)
    def mass(self,remaining,state):
        if remaining<0:return 0.0
        if not remaining:return self.probabilities.get(state,{}).get('$',0.0)
        return sum(p*self.mass(remaining-1,self.advance(state,ch)) for ch,p in self.probabilities.get(state,{}).items() if ch!='$')
    def normalizer(self,length):
        p=self.mass(length,self.start)
        return self.quantize(-math.log2(p)) if p else INF
    @lru_cache(maxsize=None)
    def terminal_cost(self,state):
        p=self.mass(4,state)
        return self.quantize(-math.log2(p)) if p else INF
    @lru_cache(maxsize=None)
    def min_cost(self,depth,state):
        if not depth:return self.terminal_cost(state)
        return min((self.costs[state][ch]+self.min_cost(depth-1,self.advance(state,ch)) for ch in self.transitions.get(state,()) if ch!='$'),default=INF)
    def prefix_cost(self,prefix):
        state=self.start;cost=0
        for ch in prefix:
            if ch not in self.costs.get(state,{}):return None
            cost+=self.costs[state][ch];state=self.advance(state,ch)
        terminal=self.terminal_cost(state)
        return None if terminal==INF else cost+terminal
    def enumerate_cost(self,depth,bound,exact=False,limit=None):
        emitted=0
        def visit(prefix,state,cost,left):
            nonlocal emitted
            if limit is not None and emitted>=limit:return
            if cost+self.min_cost(left,state)>bound:return
            if not left:
                total=cost+self.terminal_cost(state)
                if total==bound or not exact and total<bound:
                    emitted+=1;yield total,prefix
                return
            for ch in self.transitions.get(state,()):
                if ch!='$':yield from visit(prefix+ch,self.advance(state,ch),cost+self.costs[state][ch],left-1)
        yield from visit('',self.start,0,depth)
    def build_histograms(self,max_depth=20,max_cost=1024,cap=1_000_001,cc=_P.CC):
        nodes=sorted(self.transitions);ids={s:i for i,s in enumerate(nodes)}
        lines=[f'{len(nodes)} {max_depth} {max_cost} {cap} {ids[self.start]}']
        for state in nodes:
            edges=[ch for ch in self.transitions[state] if ch!='$']
            terminal=self.terminal_cost(state)
            lines.append(f'{terminal if terminal<INF else -1} {len(edges)}')
            lines.extend(f'{ids[self.advance(state,ch)]} {self.costs[state][ch]}' for ch in edges)
        encoded='\n'.join(lines)+'\n'
        key=hashlib.sha256(encoded.encode()+(_P.SRC/'prefix_cost_dp.c').read_bytes()).hexdigest()
        cache=HERE/'rank_cache';cache.mkdir(exist_ok=True)
        source=cache/(key+'.model');text_once(source,encoded)
        output=cache/(key+'.hist');exe=compiled(_P.SRC/'prefix_cost_dp.c',cache,cc,('-O3','-Wall','-Wextra'))
        run_to_file([str(exe),str(source)],output)
        hist=[[0]*(max_cost+1) for _ in range(max_depth+1)]
        for line in output.read_text().splitlines():
            d,c,n=map(int,line.split());hist[d][c]=n
        self._hist=[]
        for row in hist:
            running=0;cdf=[]
            for value in row:running=min(cap,running+value);cdf.append(running)
            self._hist.append(cdf)
        self.hist_cap=cap;self.hist_cost=max_cost;self.hist_depth=max_depth;self.hist_key=key
        return self
    def count_cost(self,depth,bound):
        if bound<0:return 0
        if depth>self.hist_depth:raise ValueError('Histogram depth exceeded')
        row=self._hist[depth]
        if bound>self.hist_cost and row[-1]<self.hist_cap:
            # Outside the stored score region is safe only when all support was
            # counted. Otherwise a silent underestimate could overstate recall.
            if row[-1]!=self.prefix_count(depth+4):raise ValueError('Histogram cost range insufficient')
        return row[min(bound,self.hist_cost)]

def head_scores(model,codes,moves,length):
    """The previously measured head prior, now exposed as deterministic weights."""
    votes=co.Counter()
    for move in sorted(moves):votes.update(model.catvotes.get(signature(move),{}))
    base=co.Counter()
    for code in sorted(codes):
        total=sum(model.head_code[code].values())
        if total:
            for pair,count in sorted(model.head_code[code].items()):base[pair]+=count/total/max(1,len(codes))
    gt=sum(model.head_global.values());catbase=co.Counter()
    for pair,count in sorted(model.head_global.items()):base[pair]+=.01*count/gt;catbase[pair[1]]+=base[pair]
    totalvotes=sum(votes.values());out=[]
    for pair,count in sorted(model.head_global.items()):
        f,c=pair
        if len(f)+len(c)+2>=length:continue
        catfactor=(votes[c]/totalvotes+.05*catbase[c])/(1.05*catbase[c]) if totalvotes else 1
        lengths=model.head_lengths[pair]
        lengthfactor=(lengths[length]+.5*(lengths[length-1]+lengths[length+1])+.01+model.global_lengths[length]/gt)/(2*count+1.01)
        out.append((base[pair]*catfactor*lengthfactor,f+'_'+c+'_'))
    return sorted(out,key=lambda x:(-x[0],x[1]))

class PrefixPlan:
    def __init__(self,model,language,codes,moves,length,head_scorer=None):
        if not 13<=length<=24:raise ValueError('Exact tail solver supports lengths 13..24')
        self.language=language;self.length=length
        scorer=head_scorer or head_scores
        weighted=[(w,h) for w,h in scorer(model,codes,moves,length) if language.mass(length-len(h),language.start)>0]
        total=sum(w for w,h in weighted);self.heads={}
        for weight,head in weighted:
            rest_length=length-len(head);headcost=language.quantize(-math.log2(weight/total))
            self.heads[head]=dict(depth=max(0,rest_length-4),rest_length=rest_length,
                offset=headcost-language.normalizer(rest_length) if rest_length>=4 else headcost,
                singleton=rest_length<4)
        self.alphabet=''.join(sorted(set(language.alphabet)|set(''.join(self.heads))))
        self.count_groups=co.Counter((v['depth'],v['offset'],v['singleton']) for v in self.heads.values())
    def count_leq(self,score,cap=None):
        cap=self.language.hist_cap if cap is None else min(cap,self.language.hist_cap)
        n=0
        for (depth,offset,singleton),multiplicity in self.count_groups.items():
            n+=multiplicity*(int(offset<=score) if singleton else self.language.count_cost(depth,score-offset))
            if n>=cap:return cap
        return n
    def truth_bounds(self,name):
        p=name.split('_',2)
        if len(p)!=3:return dict(prefix_supported=False,grammar_supported=False)
        head=p[0]+'_'+p[1]+'_';rest=p[2];info=self.heads.get(head)
        strict=self.language.accepts(rest)
        if not info or any(c not in self.alphabet for c in name[-4:]):return dict(prefix_supported=False,grammar_supported=False)
        cost=0 if info['singleton'] else self.language.prefix_cost(rest[:-4] if len(rest)>=4 else '')
        if cost is None:return dict(prefix_supported=False,grammar_supported=strict)
        score=cost+info['offset']
        if info['singleton']:
            # With fewer than four remainder bytes the solved tail includes
            # part of the head. Different allowed heads can therefore propose
            # the same physical n-4 prefix. The earliest such proposal is
            # enough: execution validates against every allowed head.
            physical=head[:self.length-4]
            score=min(v['offset'] for h,v in self.heads.items()
                      if v['singleton'] and h[:self.length-4]==physical)
        below=self.count_leq(score-1);through=self.count_leq(score)
        return dict(prefix_supported=True,grammar_supported=strict,score=score,
                    rank_lower=min(self.language.hist_cap,1+below),rank_upper=through,
                    rank_censored=through>=self.language.hist_cap)
    def cutoff(self,budget):
        if not 0<budget<self.language.hist_cap:raise ValueError('Budget must be positive and below histogram saturation cap')
        minimum=min((v['offset']+(0 if v['singleton'] else self.language.min_cost(v['depth'],self.language.start)) for v in self.heads.values()),default=0)
        high=max((v['offset']+(0 if v['singleton'] else self.language.hist_cost) for v in self.heads.values()),default=0)
        available=self.count_leq(high);wanted=min(budget,available)
        if not wanted:return dict(requested=budget,selected=0)
        lo=minimum;hi=high
        while lo<hi:
            mid=(lo+hi)//2
            if self.count_leq(mid)>=wanted:hi=mid
            else:lo=mid+1
        return dict(requested=budget,selected=wanted,cutoff_cost=lo,
                    below_cutoff=self.count_leq(lo-1),through_cutoff=self.count_leq(lo),
                    quantization_bits=1/self.language.quantum,histogram_key=self.language.hist_key)
    def select(self,budget):
        accounting=self.cutoff(budget);wanted=accounting['selected']
        if not wanted:return [],accounting
        cutoff=accounting['cutoff_cost'];items=[]
        for head,v in sorted(self.heads.items()):
            if v['singleton']:
                if v['offset']<cutoff:items.append((v['offset'],head,'',head[:self.length-4]))
            else:
                for cost,prefix in self.language.enumerate_cost(v['depth'],cutoff-v['offset']-1):
                    items.append((cost+v['offset'],head,prefix,head+prefix))
        assert len(items)==self.count_leq(cutoff-1)
        items.sort()
        for head,v in sorted(self.heads.items()):
            if len(items)>=wanted:break
            if v['singleton']:
                if v['offset']==cutoff:items.append((cutoff,head,'',head[:self.length-4]))
            else:
                for cost,prefix in self.language.enumerate_cost(v['depth'],cutoff-v['offset'],True,wanted-len(items)):
                    items.append((cutoff,head,prefix,head+prefix))
        assert len(items)==wanted
        accounting['distinct_prefixes']=len({x[3] for x in items})
        return items,accounting
    def select_native(self,budget,cc=_P.CC):
        accounting=self.cutoff(budget)
        if not accounting['selected']:return [],accounting
        language=self.language
        from parallel_prefixes import prepare_native
        model,exe=prepare_native(language,cc)
        cache=HERE/'rank_cache';cache.mkdir(exist_ok=True)
        lines=[f'{self.length} {accounting["cutoff_cost"]} {accounting["selected"]} {len(self.heads)}']
        lines.extend(f'{h} {v["depth"]} {v["offset"]} {int(v["singleton"])}' for h,v in sorted(self.heads.items()))
        data='\n'.join(lines)+'\n';key=hashlib.sha256(data.encode()).hexdigest();request=cache/(key+'.request')
        text_once(request,data)
        run=subprocess.run([str(exe),str(model),str(request)],capture_output=True,text=True,check=True)
        items=[]
        for line in run.stdout.splitlines():
            score,head,prefix,full=line.split('\t');items.append((int(score),head,prefix,full))
        items.sort();assert len(items)==accounting['selected']
        assert sum(x[0]<accounting['cutoff_cost'] for x in items)==accounting['below_cutoff']
        accounting.update(distinct_prefixes=len({x[3] for x in items}),enumerator='native')
        return items,accounting
