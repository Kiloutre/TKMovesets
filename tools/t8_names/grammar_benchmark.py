"""Leakage-controlled grammar ablations: recall versus candidate-space size.

No hash search is performed here. The labels are actual supplied known names.
Both an exact-name split and a stricter shared-family split are evaluated.
Counts are exact finite Cartesian-space counts, computed without enumeration.
Length-conditioned rows use an explicit oracle-length assumption.
"""
import collections as co
import csv
import difflib
import hashlib
import json
import math
import re
import statistics
from tail_solver import HERE
from recover import read,body,variants
from char_grammar import CharacterGrammar

def parts(name):
    p=name.split('_',2)
    return p if len(p)==3 else None

def family(name):
    p=parts(name)
    if not p:return name.lower()
    rest=p[2].lower()
    # Group character/animator substitutions, numeric variants and terminal
    # companion/role/side variants. This is stricter, not a perfect semantic
    # family partition: abbreviations and synonyms can still cross groups.
    rest=re.sub(r'(?:_(?:c|f|y|n|l|r|end|start|loop))+$','',rest)
    rest=re.sub(r'\d+','#',rest)
    return p[1].lower()+'_'+rest

def heldout(key):return int.from_bytes(hashlib.sha256(('kamui-method-v1:'+key).encode()).digest()[:8],'big')%5==0

def signature(move):
    prefix=move.split('_',1)[0]
    token=body(move).split('_',1)[0].lower()
    token=re.sub(r'lp|rp|lk|rk|wp|wk','B',token)
    token=re.sub(r'\d+','#',token)
    return (prefix if prefix in ('Co','sDm','wDm','Dm') else 'character',token)

class Model:
    def __init__(self,names,mappings):
        self.names=set(names);self.first=set();self.cats=set();self.rest=set()
        self.restcat=co.defaultdict(set);self.pc=co.defaultdict(set);self.shared=set()
        self.owners=co.defaultdict(set);self.bybody=co.defaultdict(set);self.role_samples=set()
        self.ruleexamples=co.defaultdict(set);self.catvotes=co.defaultdict(co.Counter)
        suffix=co.Counter();seen=set()
        for name in sorted(names):
            p=parts(name)
            if not p:continue
            f,c,r=p;self.first.add(f);self.cats.add(c);self.rest.add(r);self.pc[f].add(c);self.restcat[c].add(r)
            mt=re.search(r'(_(?:[a-z]{1,5}|\d{1,2})|\d{1,2})$',r)
            if mt:suffix[mt[0]]+=1
        self.tails={''}|{s for s,_ in sorted(suffix.items(),key=lambda x:(-x[1],x[0]))[:16]}
        for row in mappings:
            name=row['anim_name']
            if name not in self.names:continue
            self.owners[name].add(row['code'])
            move=row['move_name'];p=parts(name)
            if move and p:self.role_samples.add((row['code'],move,name))
            if not move or not p or (move,name) in seen:continue
            seen.add((move,name));f,c,rest=p;a=body(move).lower()
            self.bybody[a].add(rest);self.catvotes[signature(move)][c]+=1
            if not(len(f)==5 and f[:3]==row['code']):continue
            mt=difflib.SequenceMatcher(None,a,rest.lower(),autojunk=False).find_longest_match()
            if mt.size<3:continue
            key=(a[:mt.a],a[mt.a+mt.size:],rest[:mt.b],rest[mt.b+mt.size:])
            if len(key[0])+len(key[1])<=30:self.ruleexamples[key].add(a)
        self.rules={k:v for k,v in self.ruleexamples.items() if len(v)>=3}
        self.shared={name.split('_')[0] for name,owners in self.owners.items() if len(owners)>=13}&self.first
        self.restlens={c:co.Counter(map(len,rr)) for c,rr in self.restcat.items()}
        self.head_global=co.Counter();self.head_code=co.defaultdict(co.Counter)
        self.head_lengths=co.defaultdict(co.Counter);self.global_lengths=co.Counter()
        for name in sorted(self.names):
            p=parts(name)
            if not p:continue
            pair=tuple(p[:2]);self.head_global[pair]+=1;self.head_lengths[pair][len(name)]+=1;self.global_lengths[len(name)]+=1
            for code in self.owners[name]:self.head_code[code][pair]+=1
    def ranked_heads(self,codes,moves,length):
        """Rank all observed head/category pairs; no code-prefix hard exclusion."""
        votes=co.Counter()
        for move in moves:votes.update(self.catvotes.get(signature(move),{}))
        base=co.Counter()
        for code in codes:
            total=sum(self.head_code[code].values())
            if total:
                for pair,count in self.head_code[code].items():base[pair]+=count/total/max(1,len(codes))
        gt=sum(self.head_global.values());catbase=co.Counter()
        for pair,count in self.head_global.items():
            base[pair]+=.01*count/gt;catbase[pair[1]]+=base[pair]
        totalvotes=sum(votes.values());scored=[]
        for pair,count in self.head_global.items():
            f,c=pair
            if len(f)+len(c)+2>=length:continue
            catfactor=(votes[c]/totalvotes+.05*catbase[c])/(1.05*catbase[c]) if totalvotes else 1
            lengths=self.head_lengths[pair]
            lengthfactor=(lengths[length]+.5*(lengths[length-1]+lengths[length+1])+.01+self.global_lengths[length]/gt)/(2*count+1.01)
            scored.append((base[pair]*catfactor*lengthfactor,pair))
        return [pair for score,pair in sorted(scored,key=lambda x:(-x[0],x[1]))]
    def heads(self,codes):
        if len(codes)>=13:return self.shared
        return self.shared|{f for f in self.first if len(f)==5 and f[:3] in codes}
    def move_roots(self,moves):
        roots=set();votes=co.Counter()
        for move in moves:
            a=body(move).lower();roots.update(variants(move));roots.update(self.bybody.get(a,()))
            votes.update(self.catvotes.get(signature(move),{}))
            for (start,end,ns,ne),examples in self.rules.items():
                if a.startswith(start) and (not end or a.endswith(end)):
                    middle=a[len(start):len(a)-len(end) if end else len(a)]
                    if len(middle)>=2:roots.add(ns+middle+ne)
        return {r+s for r in roots for s in self.tails}, {c for c,_ in votes.most_common(3)} if votes else self.cats

def main():
    known=read('known_anim_names.csv');mapping=read('known_move_to_anim.csv')
    names={x['anim_name'] for x in known};context=co.defaultdict(lambda:dict(codes=set(),moves=set()))
    for row in mapping:
        if row['anim_name'] in names:
            x=context[row['anim_name']];x['codes'].add(row['code'])
            if row['move_name']:x['moves'].add(row['move_name'])
    output=[];splitmeta=[]
    for split,keyfn in (('name_hidden',lambda x:x),('family_hidden',family)):
        test={n for n in names if heldout(keyfn(n))};train=names-test
        assert not {keyfn(n) for n in train}&{keyfn(n) for n in test}
        model=Model(train,mapping)
        chars={order:CharacterGrammar(model.rest,order) for order in (2,3)}
        globalpairs={(f,c) for f,cats in model.pc.items() for c in cats}
        components=co.Counter()
        eligible=sorted(test&context.keys())
        splitmeta.append(dict(split=split,train_names=len(train),hidden_names=len(test),evaluated_names=len(eligible),
                              hidden_names_without_context=len(test)-len(eligible),learned_rules=len(model.rules)))
        for ordinal,name in enumerate(eligible):
            ctx=context[name];p=parts(name);L=len(name)
            first,cat,rest=p if p else ('','','')
            heads=model.heads(ctx['codes']);pairs={(f,c) for f in heads for c in model.pc.get(f,())}
            roots,predcats=model.move_roots(sorted(ctx['moves']));rootlens=co.Counter(map(len,roots))
            categorypairs={(f,c) for f,c in pairs if c in predcats}
            ranked=model.ranked_heads(ctx['codes'],ctx['moves'],L)
            components['valid_three_part_form']+=bool(p)
            components['first_token_seen']+=first in model.first
            components['code_heads_preserved']+=first in heads
            components['global_head_category_preserved']+=(first,cat) in globalpairs
            components['context_head_category_preserved']+=(first,cat) in pairs
            components['top3_category_preserved']+=cat in predcats
            def row(stage,count,retained,length=False):
                output.append(dict(split=split,name=name,codes=' '.join(sorted(ctx['codes'])),stage=stage,
                                   candidates=count,truth_retained=int(bool(retained)),length_conditioned=int(length),
                                   candidate_bits=math.log2(count) if count else 0,
                                   expected_chance_matches=count/2**32))
            globalcount=len(model.first)*len(model.cats)*len(model.rest)
            row('01_global_dictionary',globalcount,first in model.first and cat in model.cats and rest in model.rest)
            row('02_code_heads',len(heads)*len(model.cats)*len(model.rest),first in heads and cat in model.cats and rest in model.rest)
            row('03_joint_head_category',sum(len(model.restcat[c]) for f,c in pairs),(first,cat) in pairs and rest in model.restcat[cat])
            row('04_joint_plus_length',sum(model.restlens[c][L-len(f)-len(c)-2] for f,c in pairs),(first,cat) in pairs and rest in model.restcat[cat],True)
            row('05_move_grammar',len(pairs)*len(roots),(first,cat) in pairs and rest in roots)
            row('06_move_plus_length',sum(rootlens[L-len(f)-len(c)-2] for f,c in pairs),(first,cat) in pairs and rest in roots,True)
            row('07_move_length_top3_categories',sum(rootlens[L-len(f)-len(c)-2] for f,c in categorypairs),(first,cat) in categorypairs and rest in roots,True)
            # This union restores transferable dictionary names while retaining
            # generated move spellings that were absent from the training names.
            overlaps={c:co.Counter(len(r) for r in model.restcat[c]&roots) for c in predcats}
            unioncount=sum(rootlens[L-len(f)-len(c)-2]+model.restlens[c][L-len(f)-len(c)-2]-overlaps[c][L-len(f)-len(c)-2] for f,c in categorypairs)
            row('08_union_length_top3_categories',unioncount,(first,cat) in categorypairs and (rest in model.restcat[cat] or rest in roots),True)
            for order in (2,3):
                language=chars[order]
                row(f'09_char{order}_global_heads',sum(language.count(L-len(f)-len(c)-2) for f,c in globalpairs),
                    (first,cat) in globalpairs and language.accepts(rest),True)
                row(f'10_char{order}_context_heads',sum(language.count(L-len(f)-len(c)-2) for f,c in pairs),
                    (first,cat) in pairs and language.accepts(rest),True)
            row('11_char3_context_top3_categories',sum(chars[3].count(L-len(f)-len(c)-2) for f,c in categorypairs),
                (first,cat) in categorypairs and chars[3].accepts(rest),True)
            for budget in (1,3,10,30):
                selected=set(ranked[:budget])
                row(f'12_char3_ranked_heads_{budget:02}',sum(chars[3].count(L-len(f)-len(c)-2) for f,c in selected),
                    (first,cat) in selected and chars[3].accepts(rest),True)
                components[f'head_rank_top_{budget}']+=(first,cat) in selected
            if ordinal%500==0:print('progress',split,ordinal,'of',len(eligible),flush=True)
        print('complete',split,'evaluated',len(eligible),'rules',len(model.rules),flush=True)
        splitmeta[-1]['component_coverage_counts']=dict(components)
    with (HERE/'grammar_per_name.csv').open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,list(output[0]));w.writeheader();w.writerows(output)
    summaries=[]
    for split in ('name_hidden','family_hidden'):
        for cohort in ('all','inu','shared'):
            selected=[x for x in output if x['split']==split and (cohort=='all' or (x['codes']=='inu' if cohort=='inu' else len(x['codes'].split())>=13))]
            for stage in sorted({x['stage'] for x in selected}):
                rows=[x for x in selected if x['stage']==stage];counts=sorted(x['candidates'] for x in rows)
                summaries.append(dict(split=split,cohort=cohort,stage=stage,test_names=len(rows),
                                      retained=sum(x['truth_retained'] for x in rows),recall=sum(x['truth_retained'] for x in rows)/len(rows),
                                      candidates_median=statistics.median(counts),candidates_mean=statistics.mean(counts),
                                      candidates_p95=counts[math.ceil(.95*len(counts))-1],
                                      candidate_space_bits_median=statistics.median(x['candidate_bits'] for x in rows)))
    with (HERE/'grammar_summary.csv').open('w',encoding='utf-8',newline='') as f:
        w=csv.DictWriter(f,list(summaries[0]));w.writeheader();w.writerows(summaries)
    (HERE/'grammar_validation.json').write_text(json.dumps(dict(splits=splitmeta,known_names=len(names),
        split_salt='kamui-method-v1',test_fraction='hash modulo 5 equals zero',
        leakage_checks='Training excludes every hidden exact name; family split also excludes matching normalized family keys.',
        limitations=['Length is conditional/oracle information, not predicted from the hash.',
                     'Holdout is retrospective: naming conventions were already inspected during development.',
                     'Normalized families do not catch every synonym or abbreviation.',
                     'Known-name holdouts are easier than the residual unresolved set.',
                     'These are candidate-space/recall tests, not hash searches; no new names are accepted.']),indent=2),encoding='utf-8')

if __name__=='__main__':main()
