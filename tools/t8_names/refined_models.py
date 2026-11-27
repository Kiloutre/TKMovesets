"""Ablations motivated by measured rare-context and head-length failures."""
import collections as co
from functools import lru_cache
from long_models import GraphGrammar,SubwordGrammar
from ranked_prefixes import PrefixPlan,head_scores
from grammar_benchmark import signature


class AdaptiveGrammar(GraphGrammar):
    """Six-gram graph with count-dependent high-order interpolation.

    A rare history no longer assigns 75% to its few observed continuations.
    The four-observation prior is fixed, not fitted on heldout rank outcomes.
    """
    def __init__(self,words,prior_strength=4,max_weight=.95,quantum=8):
        words=sorted(set(words));self.model_id='adaptive6';self.order=6;self.quantum=quantum;self.backoff=.1
        self.start='^^^^^';self.alphabet=''.join(sorted(set(''.join(words))));counts=co.defaultdict(co.Counter)
        assert not set(self.alphabet)&{'^','$','?'}
        for word in words:
            history=self.start
            for ch in word+'$':
                for n in range(1,6):counts[history[-n:]][ch]+=1
                history=(history+ch)[-5:]
        @lru_cache(None)
        def distribution(state):
            counter=counts[state];total=sum(counter.values())
            if len(state)==1:return {ch:n/total for ch,n in counter.items()}
            back=distribution(state[1:]);weight=.9 if len(state)==2 else min(max_weight,total/(total+prior_strength))
            return {ch:weight*counter[ch]/total+(1-weight)*back.get(ch,0) for ch in set(counter)|set(back)}
        def advance(state,ch):
            history=(state+ch)[-5:]
            while history not in counts:history=history[1:]
            return history
        self.model_details=dict(model=self.model_id,unique_training_rests=len(words),max_history=5,
                                prior_strength=prior_strength,max_weight=max_weight,trigram_weight=.9,quantum=quantum)
        self.build_graph(distribution,advance)


def pooled_head_scores(model,codes,moves,length,strength=20):
    """Keep code/category evidence; smooth sparse animator-specific lengths.

    Replace only the old length factor. Pool remainder lengths by category,
    backing category counts off to all training names. Heads remain observed
    head/category pairs; no new identifiers or truth-derived words are added.
    """
    if not hasattr(model,'_pooled_length_data'):
        cats=co.defaultdict(co.Counter);global_lengths=co.Counter()
        for name in sorted(model.names):
            p=name.split('_',2)
            if len(p)==3:cats[p[1]][len(p[2])]+=1;global_lengths[len(p[2])]+=1
        model._pooled_length_data=(cats,global_lengths,{c:sum(v.values()) for c,v in cats.items()},sum(global_lengths.values()),max(global_lengths)+2)
    cats,glob,cat_totals,gt_rest,width=model._pooled_length_data
    gt=sum(model.head_global.values());result=[]
    kernel=lambda counts,n:.5*counts[n]+.25*(counts[n-1]+counts[n+1])
    for weight,head in head_scores(model,codes,moves,length):
        first,cat,_=head.split('_',2);pair=(first,cat);count=model.head_global[pair];hist=model.head_lengths[pair]
        old=(hist[length]+.5*(hist[length-1]+hist[length+1])+.01+model.global_lengths[length]/gt)/(2*count+1.01)
        remaining=length-len(head)
        global_prior=(kernel(glob,remaining)+1/width)/(gt_rest+1)
        cat_prior=(kernel(cats[cat],remaining)+strength*global_prior)/(cat_totals[cat]+strength)
        local=(kernel(hist,length)+strength*cat_prior)/(count+strength)
        result.append((weight*local/old,head))
    return sorted(result,key=lambda x:(-x[0],x[1]))


def role_head_scores(model,codes,moves,length,strength=20,ratio_limit=8):
    """Learn move-pattern evidence for own-character/shared/foreign heads.

    Smooth exact move signatures toward their broad move class, then category
    role frequencies. Apply bounded likelihood ratios to the unchanged prior.
    """
    def role(first,code):
        if first[:3].lower()==code.lower():return 'own'
        return 'shared' if first in model.shared else 'foreign'
    if not hasattr(model,'_role_counts'):
        prior=co.defaultdict(co.Counter);broad=co.defaultdict(co.Counter);fine=co.defaultdict(co.Counter);seen=set()
        for code,move,name in sorted(model.role_samples):
            first,cat,_=name.split('_',2);r=role(first,code);sig=signature(move)
            broad[cat,sig[0]][r]+=1;fine[cat,sig][r]+=1
            if (code,name) not in seen:prior[cat][r]+=1;seen.add((code,name))
        model._role_counts=prior,broad,fine
    prior,broad,fine=model._role_counts;roles=('own','shared','foreign');factors={}
    sigs=sorted({signature(m) for m in moves if m})
    for cat in model.cats:
        pc=prior[cat];total=sum(pc.values());base={r:(pc[r]+1)/(total+3) for r in roles}
        values=co.Counter()
        for sig in sigs:
            bc=broad[cat,sig[0]];fc=fine[cat,sig];bn=sum(bc.values());fn=sum(fc.values())
            for r in roles:
                bp=(bc[r]+strength*base[r])/(bn+strength)
                p=(fc[r]+strength*bp)/(fn+strength)
                values[r]+=p/len(sigs)
        factors[cat]={r:max(1/ratio_limit,min(ratio_limit,values[r]/base[r])) if sigs else 1 for r in roles}
    result=[]
    for weight,head in head_scores(model,codes,moves,length):
        first,cat,_=head.split('_',2)
        factor=sum(factors[cat][role(first,code)] for code in codes)/len(codes) if codes else 1
        result.append((weight*factor,head))
    return sorted(result,key=lambda x:(-x[0],x[1]))


REFINED_MODELS=('subword6','subword6_pooled','adaptive6','adaptive6_pooled','subword6_roles','subword9','subword9_roles')

def refined_plan(model,language,codes,moves,length,kind):
    assert kind in REFINED_MODELS
    scorer=pooled_head_scores if kind.endswith('_pooled') else role_head_scores if kind.endswith('_roles') else None
    plan=PrefixPlan(model,language,codes,moves,length,scorer)
    plan.model_id=kind
    return plan


def refined_language(kind,words):
    if kind.startswith('subword9'):
        language=SubwordGrammar(words,max_history=8);language.model_id='subword9';language.model_details['model']='subword9'
        return language
    return AdaptiveGrammar(words) if kind.startswith('adaptive6') else SubwordGrammar(words)
