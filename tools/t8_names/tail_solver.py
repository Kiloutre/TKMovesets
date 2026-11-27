"""Exact final-word solving for Kamui's 13..24-byte branch.

For n>=15 the final four bytes occur only at the last step. For n=13 or 14,
one of those bytes also occurs earlier: condition on that byte and check it.
This reduces enumeration work, not ambiguity between hash preimages.
"""
import t8names_paths as _P
from pathlib import Path
import sys

HERE = _P.LAB
from inverse import digest,unfmix,unstep,unmix,step,mix,rd

def prefix_state(data,n):
    assert 13<=n<=24 and len(data)>=n-4+(n<15)
    value=n
    for i,offset in enumerate((n//2-4,4,n-8,n//2,0)):
        value=step(value,mix(rd(data,offset)),i in (1,3))
    return value

def solve(prefix,target,alphabet=b'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789',tail_pattern=b'????'):
    """Yield every alphabet-constrained name with this fixed n-4-byte prefix."""
    if isinstance(prefix,str):prefix=prefix.encode('ascii')
    n=len(prefix)+4
    if not 13<=n<=24:raise ValueError('Supported total lengths are 13..24')
    if len(tail_pattern)!=4:raise ValueError('Tail pattern must contain four bytes')
    allowed=set(alphabet)
    end=unstep(unfmix(target),0)
    boundary=alphabet if tail_pattern[0]==ord('?') else tail_pattern[:1]
    for overlap in (boundary if n<15 else (None,)):
        data=prefix+(bytes([overlap]) if overlap is not None else b'')
        tail=unmix(prefix_state(data,n)^end).to_bytes(4,'little')
        if overlap is not None and tail[0]!=overlap:continue
        if all(x in allowed if p==ord('?') else x==p for x,p in zip(tail,tail_pattern)):
            name=prefix+tail
            assert digest(name)==target
            yield name

if __name__=='__main__':
    import argparse
    ap=argparse.ArgumentParser();ap.add_argument('target',type=lambda s:int(s,16));ap.add_argument('prefix');a=ap.parse_args()
    for name in solve(a.prefix,a.target):print(name.decode('ascii'))
