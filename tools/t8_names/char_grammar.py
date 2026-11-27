"""Finite character grammar with exact counts and suffix-completable prefixes."""
import collections as co
from functools import lru_cache

class CharacterGrammar:
    def __init__(self,words,order=3):
        if order not in (2,3):raise ValueError('Supported orders: 2, 3')
        self.order=order;self.start='^'*(order-1);self.transitions=co.defaultdict(set)
        self.alphabet=set()
        for word in sorted(set(words)):
            assert not set(word)&{'^','$','?'}
            state=self.start;self.alphabet.update(word)
            for ch in word+'$':
                self.transitions[state].add(ch);state=(state+ch)[-(order-1):]
        self.transitions={state:tuple(sorted(chars)) for state,chars in self.transitions.items()}
        self.alphabet=''.join(sorted(self.alphabet))
    def advance(self,state,ch):
        return (state+ch)[-(self.order-1):]
    def accepts(self,word):
        state=self.start
        for ch in word+'$':
            if ch not in self.transitions.get(state,()):return False
            if ch!='$':state=self.advance(state,ch)
        return True
    @lru_cache(maxsize=None)
    def completions(self,remaining,state):
        if remaining<0:return 0
        if remaining==0:return int('$' in self.transitions.get(state,()))
        return sum(self.completions(remaining-1,self.advance(state,ch))
                   for ch in self.transitions.get(state,()) if ch!='$')
    def count(self,length):return self.completions(length,self.start)
    @lru_cache(maxsize=None)
    def prefix_count(self,total_length,solved_tail=4):
        if total_length<0:return 0
        if total_length<=solved_tail:return int(self.count(total_length)>0)
        states={self.start:1}
        for i in range(total_length-solved_tail):
            nextstates=co.Counter()
            for state,count in states.items():
                for ch in self.transitions.get(state,()):
                    if ch!='$':nextstates[self.advance(state,ch)]+=count
            states=nextstates
        return sum(count for state,count in states.items() if self.completions(solved_tail,state))
    def prefixes(self,total_length,solved_tail=4):
        """Yield only prefixes having at least one accepted fixed-length suffix."""
        if total_length<0:return
        if total_length<=solved_tail:
            if self.count(total_length):yield ''
            return
        wanted=total_length-solved_tail
        def visit(prefix,state):
            if not self.completions(total_length-len(prefix),state):return
            if len(prefix)==wanted:yield prefix;return
            for ch in self.transitions.get(state,()):
                if ch!='$':yield from visit(prefix+ch,self.advance(state,ch))
        yield from visit('',self.start)
