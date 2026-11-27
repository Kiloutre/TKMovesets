"""Generate uncertain animation heads; never require the true head as input.

    observed head/category prior + code/initial/category composition
The budgets here are model pruning parameters, not supplied name prefixes.
Every retained head still competes inside the total prefix-proposal budget.
"""
import collections as co
import math
import string
from functools import lru_cache
from ranked_prefixes import PrefixPlan,head_scores
from refined_models import refined_language


class OpenHeadScorer:
    def __init__(self,model,initial_budget=128,head_budget=8192,seen_mix=.25,new_mix=.85):
        self.model=model;self.initial_budget=initial_budget;self.head_budget=head_budget
        self.seen_mix=seen_mix;self.new_mix=new_mix
        firsts={f for f in model.first if len(f)==5 and f.isascii() and f.isalpha()}
        counts=co.Counter(f[3:] for f in firsts);left=co.Counter();right=co.Counter()
        for initial,n in counts.items():left[initial[0]]+=n;right[initial[1]]+=n
        total=sum(counts.values());letters=string.ascii_letters
        pairs=[]
        for a in letters:
            for b in letters:
                # Positive support before explicit top-128 model pruning.
                back=(left[a]+.1)/(total+5.2)*(right[b]+.1)/(total+5.2)
                p=.9*counts[a+b]/max(1,total)+.1*back
                pairs.append((p,a+b))
        pairs.sort(key=lambda x:(-x[0],x[1]));self.initials=pairs[:initial_budget]
        z=sum(p for p,i in self.initials);self.initials=[(p/z,i) for p,i in self.initials]
        codes={code for code,move,name in model.role_samples}
        stemcounts=co.Counter(f[:3] for f in model.shared if len(f)==5 and f.isascii() and f.isalpha() and f[:3] not in codes)
        self.shared_stems={s:n for s,n in stemcounts.items() if n>=2}
        self.known_stems={f[:3] for f in firsts}
        self.generated_initials={i for p,i in self.initials};self.observed_initials=set(counts)
        self.details=dict(initial_pair_budget=initial_budget,generated_head_budget=head_budget,seen_code_mixture=seen_mix,
                          unseen_code_mixture=new_mix,selected_initials=len(self.initials),
                          selected_unseen_initial_pairs=len(self.generated_initials-self.observed_initials),shared_stems=self.shared_stems)

    @lru_cache(maxsize=32)
    def templates(self,codes,length):
        # For many-character animations, shared stems get the composition pool.
        own=[c for c in codes if len(c)==3 and c.isascii() and c.isalpha()] if len(codes)<13 else []
        stems=co.Counter()
        if own:
            for c in own:stems[c]+=.9/len(own)
        common=sum(self.shared_stems.values())
        if common:
            for c,n in self.shared_stems.items():stems[c]+=(.1 if own else 1)*n/common
        if not common and own:
            for c in own:stems[c]=1/len(own)
        out=[]
        for stem,sw in sorted(stems.items()):
            for ip,initial in self.initials:
                for cat in sorted(self.model.cats):
                    head=stem+initial+'_'+cat+'_'
                    if len(head)<length:out.append((sw*ip,cat,head))
        return out

    def __call__(self,model,codes,moves,length):
        assert model is self.model
        old=head_scores(model,codes,moves,length);total=sum(p for p,h in old)
        if not total:return old
        category=co.Counter()
        for p,h in old:category[h.split('_')[1]]+=p/total
        template_codes=tuple(sorted(codes)) if len(codes)<13 else ()
        generated=[(base*category[cat],head) for base,cat,head in self.templates(template_codes,length) if category[cat]>0]
        generated.sort(key=lambda x:(-x[0],x[1]));generated=generated[:self.head_budget]
        z=sum(p for p,h in generated)
        if not z:return old
        eligible=[c for c in codes if len(c)==3 and c.isascii() and c.isalpha()]
        unseen=bool(eligible) and len(codes)<13 and all(c not in self.known_stems for c in eligible)
        mixture=self.new_mix if unseen else self.seen_mix
        weights=co.Counter({h:(1-mixture)*p/total for p,h in old})
        for p,h in generated:weights[h]+=mixture*p/z
        return sorted(((p,h) for h,p in weights.items()),key=lambda x:(-x[0],x[1]))


def open_plan(model,language,codes,moves,length,kind):
    if kind=='subword6':scorer=None
    else:
        if not hasattr(model,'_open_head_scorer'):model._open_head_scorer=OpenHeadScorer(model)
        scorer=model._open_head_scorer
    plan=PrefixPlan(model,language,codes,moves,length,scorer);plan.model_id=kind
    return plan

def open_language(kind,words):return refined_language('subword9' if kind=='open9' else 'subword6',words)
