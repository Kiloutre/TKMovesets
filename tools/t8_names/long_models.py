"""Training-only long-name languages with deterministic finite-state paths.

All rankings use the existing head prior, length normalization, four-byte tail
mass and eighth-bit costs. Neither model sees target hashes or heldout names.
"""
import collections as co
from functools import lru_cache
import math
from ranked_prefixes import WeightedGrammar


class GraphGrammar(WeightedGrammar):
    def advance(self,state,ch):return self.next_states[state][ch]
    def build_graph(self,distribution,advance):
        self.probabilities={};self.transitions={};self.costs={};self.next_states={};self._hist=None
        pending=[self.start]
        while pending:
            state=pending.pop()
            if state in self.probabilities:continue
            probs={ch:p for ch,p in sorted(distribution(state).items()) if p>0}
            assert abs(sum(probs.values())-1)<1e-10,(state,probs)
            self.probabilities[state]=probs;self.transitions[state]=tuple(probs)
            self.costs[state]={ch:self.quantize(-math.log2(p)) for ch,p in probs.items()}
            edges={ch:advance(state,ch) for ch in probs if ch!='$'}
            self.next_states[state]=edges;pending.extend(edges.values())
        self.model_details['states']=len(self.transitions)


class SubwordGrammar(GraphGrammar):
    """Up to five characters of history, backing off to observed suffix states."""
    def __init__(self,words,max_history=5,high_weight=.75,quantum=8):
        self.model_id='subword6';self.order=max_history+1;self.quantum=quantum;self.backoff=.1
        self.start='^'*max_history;words=sorted(set(words));counts=co.defaultdict(co.Counter)
        self.alphabet=''.join(sorted(set(''.join(words))))
        assert not set(self.alphabet)&{'^','$','?'}
        for word in words:
            history=self.start
            for ch in word+'$':
                for n in range(1,max_history+1):counts[history[-n:]][ch]+=1
                history=(history+ch)[-max_history:]
        @lru_cache(None)
        def distribution(state):
            counter=counts[state];total=sum(counter.values())
            if len(state)==1:return {ch:n/total for ch,n in counter.items()}
            back=distribution(state[1:]);weight=.9 if len(state)==2 else high_weight
            return {ch:weight*counter[ch]/total+(1-weight)*back.get(ch,0) for ch in set(counter)|set(back)}
        def advance(state,ch):
            history=(state+ch)[-max_history:]
            while history not in counts:history=history[1:]
            return history
        self.model_details=dict(model=self.model_id,unique_training_rests=len(words),max_history=max_history,
                                high_order_weight=high_weight,trigram_weight=.9,quantum=quantum)
        self.build_graph(distribution,advance)


class TokenTrieGrammar(GraphGrammar):
    """Reusable whole tokens plus character fallback, never a token whitelist."""
    def __init__(self,words,min_frequency=2,max_tokens=2048,lexical_weight=.9,bos_weight=.75,quantum=8):
        words=sorted(set(words));self.model_id='token_trie';self.order=3;self.quantum=quantum;self.backoff=.1
        self.start=('B','','^^');self.alphabet=''.join(sorted(set(''.join(words))))
        assert not set(self.alphabet)&{'^','$','?'}
        tri=co.defaultdict(co.Counter);bi=co.defaultdict(co.Counter);events=co.Counter();bos=co.Counter();frequency=co.Counter()
        for word in words:
            hist='^^'
            for ch in word+'$':
                tri[hist][ch]+=1;bi[hist[-1]][ch]+=1;hist=(hist+ch)[-2:]
            tokens=word.split('_')
            for i,token in enumerate(tokens):
                boundary='$' if i==len(tokens)-1 else '_'
                events[token,boundary]+=1;frequency[token]+=1
                if i==0:bos[token,boundary]+=1
        vocabulary={t for t,n in sorted(frequency.items(),key=lambda x:(-x[1],x[0])) if n>=min_frequency}
        vocabulary=set(sorted(vocabulary,key=lambda t:(-frequency[t],t))[:max_tokens])
        global_trie=co.defaultdict(co.Counter);bos_trie=co.defaultdict(co.Counter)
        for source,dest in ((events,global_trie),(bos,bos_trie)):
            total=sum(n for (token,boundary),n in source.items() if token in vocabulary)
            for (token,boundary),n in source.items():
                if token not in vocabulary:continue
                text=token+boundary
                for i,ch in enumerate(text):dest[token[:i]][ch]+=n/total
        @lru_cache(None)
        def character(hist):
            tc=tri[hist];bc=bi[hist[-1]];tn=sum(tc.values());bn=sum(bc.values())
            assert bn
            return {ch:(.9*tc[ch]/tn+.1*bc[ch]/bn if tn else bc[ch]/bn) for ch in set(tc)|set(bc)}
        @lru_cache(None)
        def distribution(state):
            ctx,prefix,hist=state;back=character(hist)
            if ctx=='F' or prefix not in global_trie:return back
            lex=global_trie[prefix]
            if ctx=='B':
                first=bos_trie.get(prefix,{})
                lex={ch:bos_weight*first.get(ch,0)+(1-bos_weight)*lex.get(ch,0) for ch in set(lex)|set(first)}
            total=sum(lex.values())
            return {ch:lexical_weight*lex.get(ch,0)/total+(1-lexical_weight)*back.get(ch,0) for ch in set(lex)|set(back)}
        def advance(state,ch):
            ctx,prefix,hist=state;hist=(hist+ch)[-2:]
            if ch=='_':return ('G','',hist)
            prefix+=ch
            if ctx=='F' or prefix not in global_trie:return ('F','',hist)
            if ctx=='B' and prefix not in bos_trie:ctx='G'
            return (ctx,prefix,hist)
        self.model_details=dict(model=self.model_id,unique_training_rests=len(words),vocabulary_size=len(vocabulary),
                                min_frequency=min_frequency,max_tokens=max_tokens,lexical_weight=lexical_weight,
                                bos_weight=bos_weight,character_trigram_weight=.9,quantum=quantum)
        self.build_graph(distribution,advance)


def make_language(model_id,words):
    if model_id=='char3':return WeightedGrammar(words,.1)
    if model_id=='subword6':return SubwordGrammar(words)
    if model_id=='token_trie':return TokenTrieGrammar(words)
    raise ValueError(model_id)


def clear_language_caches():
    # Cached methods otherwise retain every completed training model instance.
    from char_grammar import CharacterGrammar
    for method in (WeightedGrammar.mass,WeightedGrammar.terminal_cost,WeightedGrammar.min_cost,
                   CharacterGrammar.completions,CharacterGrammar.prefix_count):method.cache_clear()
