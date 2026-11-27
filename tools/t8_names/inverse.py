"""Verified inverses, including a real O(P + T*S) join for lengths 16..24.

All operations are modulo 2**32. See REPORT.txt for the dependency argument.
The 4-byte prefix must be fixed per join. Arbitrary prefix/suffix separation is
not valid: the first word occurs again late in the mixing chain.
"""
from collections import defaultdict
import json
import random
from recover import DATA, HERE, read, digest
from kamuiHash import C1,C2,MIX_SUB,MIX_SUB2,MIX_MUL1,FMIX1,FMIX2

M=0xffffffff
I5=pow(5,-1,2**32)
def u(x):return x&M
def rol(x,k):x&=M;return u(x<<k|x>>(32-k))
def ror(x,k):return rol(x,32-k)
def bswap(x):return int.from_bytes(u(x).to_bytes(4,'little'),'big')
def rd(s,i):return int.from_bytes(s[i:i+4],'little')
def mix(x):return u(rol(u(x*C1),15)*C2)
def unmix(x):return u(ror(u(x*pow(C2,-1,2**32)),15)*pow(C1,-1,2**32))
def xor_right_inverse(y,k):
    x=y
    for j in range(k,32,k):x^=y>>j
    return u(x)
def fmix(x):
    x=u((x^(x>>16))*FMIX1);x=u((x^(x>>13))*FMIX2)
    return x^(x>>16)
def unfmix(x):
    x=u(xor_right_inverse(x,16)*pow(FMIX2,-1,2**32))
    x=u(xor_right_inverse(x,13)*pow(FMIX1,-1,2**32))
    return xor_right_inverse(x,16)
def step(x,k,alt=False):return u(5*rol(x^k,13)-(MIX_SUB2 if alt else 5*MIX_SUB))
def unstep(y,k,alt=False):return ror(u((y+(MIX_SUB2 if alt else 5*MIX_SUB))*I5),13)^k

def loop_forward(v,a,b,c,d,e):
    x,y,z=v
    nx=u(a-MIX_MUL1*rol(b+z,13))
    t=u(5*(rol(x^a,14)-MIX_SUB))^u(b+d)
    ny=bswap(5*(e+rol(t,13)-MIX_SUB))
    nz=u(5*bswap(e^u(5*(rol(y+c,14)-MIX_SUB))))
    return nx,ny,nz
def loop_inverse(v,a,b,c,d,e):
    nx,ny,nz=v
    z=u(ror(u((a-nx)*pow(MIX_MUL1,-1,2**32)),13)-b)
    t=ror(u(bswap(ny)*I5-e+MIX_SUB),13)^u(b+d)
    x=ror(u(t*I5+MIX_SUB),14)^a
    t=bswap(u(nz*I5))^e
    y=u(ror(u(t*I5+MIX_SUB),14)-c)
    return x,y,z
def finish_long(v):
    x,y,z=v
    f=rol(u(C1*ror(z,11)),15)
    inner=u(x-MIX_MUL1*rol(u(C1*ror(y,11)),15))
    h=u(5*rol(u(C1*(f+rol(u(5*(rol(inner,13)-MIX_SUB)),15))),13)-MIX_SUB2)
    return u(C1*rol(h,15))
def unfinish_long(h,y,z):
    h=ror(u(h*pow(C1,-1,2**32)),15)
    h=u(ror(u((h+MIX_SUB2)*I5),13)*pow(C1,-1,2**32))
    h=u(h-rol(u(C1*ror(z,11)),15))
    inner=ror(u(ror(h,15)*I5+MIX_SUB),13)
    return u(inner+MIX_MUL1*rol(u(C1*ror(y,11)),15))

def check_name(s):
    data=s.encode('ascii') if isinstance(s,str) else s;n=len(data);h=digest(data)
    if n<=4:
        acc=0;x=9
        for byte in data:
            acc=u((byte if byte<128 else byte-256)-MIX_MUL1*acc);x^=acc
        st=unstep(unfmix(h),mix(acc))
        assert unstep(st,mix(n))==x
    elif n<=12:
        km=u(C2*rol(0x318F97D9-MIX_MUL1*rd(data,(n>>1)&4),15))
        st=unstep(unfmix(h),km)
        st=unstep(st,mix(5*n+rd(data,n-4)))
        st=unstep(st,mix(n+rd(data,0)))
        assert st==5*n
    elif n<=24:
        offsets=[n//2-4,4,n-8,n//2,0,n-4]
        st=unfmix(h)
        for i in reversed(range(6)):st=unstep(st,mix(rd(data,offsets[i])),i in (1,3))
        assert st==n
    else:
        k4,k8,k12,k16,k20=(mix(rd(data,n-d)) for d in (4,8,12,16,20))
        v=(step(step(n,k4),k16),step(step(u(C1*n),k8),k12),step(u(C1*n+k20),0))
        init=v;blocks=[]
        for o in range(0,((n-1)//20)*20,20):
            a,b,c,d,e=mix(rd(data,o)),rd(data,o+4),mix(rd(data,o+8)),mix(rd(data,o+12)),rd(data,o+16)
            blocks.append((a,b,c,d,e));v=loop_forward(v,a,b,c,d,e)
        assert finish_long(v)==h
        assert unfinish_long(h,v[1],v[2])==v[0]
        for block in reversed(blocks):v=loop_inverse(v,*block)
        assert v==init

def mitm(prefixes,suffixes,n,target):
    """Names = prefix + suffix; len(prefix)=floor(n/2), first 4 bytes fixed.
    Returns hits, forward-state evaluations, reverse-state evaluations.
    """
    assert 16<=n<=24
    mid=n//2;prefixes=sorted(set(prefixes));suffixes=sorted(set(suffixes))
    assert prefixes and len({p[:4] for p in prefixes})==1
    assert all(len(p)==mid for p in prefixes) and all(len(s)==n-mid for s in suffixes)
    table=defaultdict(list)
    for p in prefixes:
        d=p.encode('ascii')
        st=step(step(n,mix(rd(d,mid-4))),mix(rd(d,4)),True)
        table[st].append(p)
    word0=mix(rd(prefixes[0].encode('ascii'),0));out=[]
    f=unfmix(target)
    for s in suffixes:
        d=s.encode('ascii')
        st=unstep(f,mix(rd(d,n-4-mid)))
        st=unstep(st,word0)
        st=unstep(st,mix(rd(d,0)),True)
        st=unstep(st,mix(rd(d,n-8-mid)))
        for p in table.get(st,[]):
            assert digest(p+s)==target
            out.append(p+s)
    return out,len(prefixes),len(suffixes)

def selftest():
    rng=random.Random(20261115)
    for i in range(20000):
        x=rng.getrandbits(32);k=rng.getrandbits(32)
        assert unfmix(fmix(x))==x and unmix(mix(x))==x
        for alt in (False,True):assert unstep(step(x,k,alt),k,alt)==x
        v=tuple(rng.getrandbits(32) for _ in range(3));b=tuple(rng.getrandbits(32) for _ in range(5))
        assert loop_inverse(loop_forward(v,*b),*b)==v
        assert unfinish_long(finish_long(v),v[1],v[2])==v[0]
    counts=defaultdict(int);mitm_count=0
    for r in read('known_anim_names.csv'):
        s=r['anim_name'];check_name(s);n=len(s)
        counts['tiny' if n<=4 else 'short' if n<=12 else 'medium' if n<=24 else 'long']+=1
        if 16<=n<=24:
            mid=n//2
            # Include distractors in both halves, not just a one-element identity.
            ps={s[:mid],s[:4]+'a'*(mid-4),s[:4]+'z'*(mid-4)}
            ss={s[mid:],'a'*(n-mid),'z'*(n-mid)}
            hits,_,_=mitm(ps,ss,n,int(r['hash'],16))
            assert s in hits
            brute={p+t for p in ps for t in ss if digest(p+t)==int(r['hash'],16)}
            assert set(hits)==brute;mitm_count+=1
    # Known data has no tiny names. Exercise every boundary and signed bytes.
    for n in range(81):
        for i in range(10):check_name(bytes(rng.randrange(256) for _ in range(n)))
    result=dict(known_name_inverse_round_trips=dict(counts),mitm_known_names=mitm_count,
                random_arithmetic_trials=20000,binary_boundary_cases=810)
    (HERE/'inverse_validation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(result)

if __name__=='__main__':selftest()
