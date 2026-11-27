"""Deterministic parallel enumeration under one shared prefix budget.

This stage does not hash anything. Its output is exactly the serial enumerator's
head/prefix proposals, including lexicographic boundary ties. Search callers
must pass the resulting unique prefixes to the unchanged real-plus-four-decoy
search. No worker receives an additional candidate budget.
"""
import t8names_paths as _P
from concurrent.futures import ThreadPoolExecutor
import collections as co
import hashlib
import heapq
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import time
from ranked_prefixes import HERE,INF
from native_cache import text_once,publish

_BUILD_LOCK=threading.Lock()


def _atomic_text(path,text):
    """Publish complete immutable cache entries, never partially written files."""
    text_once(path,text)


def prepare_native(language,cc=_P.CC):
    """Create the graph once per language and compile once per C source hash.

    Separate, content-addressed files avoid racing serial enumeration or a
    second target job. Character labels participate in the graph cache key.
    """
    cached=getattr(language,'_parallel_enumeration_graph',None)
    cache=HERE/'rank_cache';cache.mkdir(exist_ok=True)
    if cached is None or not Path(cached).exists():
        nodes=sorted(language.transitions);ids={s:i for i,s in enumerate(nodes)}
        lines=[f'{len(nodes)} {language.hist_depth} {ids[language.start]}']
        for state in nodes:
            edges=[ch for ch in language.transitions[state] if ch!='$'];terminal=language.terminal_cost(state)
            lines.append(f'{terminal if terminal<INF else -1} {len(edges)}')
            lines.extend(f'{ids[language.advance(state,ch)]} {language.costs[state][ch]} {ord(ch)}' for ch in edges)
        data='\n'.join(lines)+'\n';key=hashlib.sha256(data.encode('ascii')).hexdigest()
        model=cache/('parallel_'+key+'.enumeration');_atomic_text(model,data)
        language._parallel_enumeration_graph=str(model)
    else:model=Path(cached)
    source=_P.SRC/'prefix_enumerate.c';source_key=hashlib.sha256(source.read_bytes()).hexdigest()[:16]
    exe=cache/('prefix_enumerate_'+source_key+'.exe')
    with _BUILD_LOCK:
        if not exe.exists():
            handle,name=tempfile.mkstemp(prefix='parallel_compile_',suffix='.exe',dir=cache);os.close(handle)
            try:
                subprocess.run([cc,'-O3','-Wall','-Wextra','-o',name,str(source)],check=True)
                publish(name,exe)
            finally:
                if os.path.exists(name):os.unlink(name)
    return model,exe


def allocate_heads(plan,accounting):
    """Allocate exact per-head counts before workers are assigned.

    Counts below the cutoff cannot be saturated, because their global sum is
    smaller than the requested budget. A saturated count through the cutoff
    still provides more than enough boundary ties for any remaining quota.
    """
    if not accounting['selected']:return []
    cutoff=accounting['cutoff_cost'];remaining=accounting['selected']-accounting['below_cutoff']
    allocated=[]
    for head,info in sorted(plan.heads.items()):
        if info['singleton']:
            below=int(info['offset']<cutoff);through=int(info['offset']<=cutoff)
        else:
            below=plan.language.count_cost(info['depth'],cutoff-info['offset']-1)
            through=plan.language.count_cost(info['depth'],cutoff-info['offset'])
        assert below<=through
        ties=min(remaining,through-below);remaining-=ties
        if below+ties:
            allocated.append(dict(head=head,info=info,below=below,ties=ties,selected=below+ties))
    assert remaining==0
    assert sum(h['below'] for h in allocated)==accounting['below_cutoff']
    assert sum(h['selected'] for h in allocated)==accounting['selected']
    return allocated


def select_parallel(plan,budget,workers=4,cc=_P.CC):
    """Return (items, accounting), compatible with PrefixPlan.select_native.

    Each item is (score, original_head, remainder_prefix, physical_prefix).
    Workers partition head proposals rather than physical prefix strings:
    distinct heads can share a truncated physical prefix, and the caller must
    keep its existing final deduplication and represented-space accounting.
    """
    if not isinstance(workers,int) or isinstance(workers,bool) or not 1<=workers<=32:
        raise ValueError('workers must be an integer from 1 to 32')
    started=time.perf_counter();accounting=plan.cutoff(budget)
    if not accounting['selected']:
        accounting.update(distinct_prefixes=0,enumerator='native_parallel',workers_requested=workers,workers_used=0,worker_jobs=[])
        return [],accounting
    heads=allocate_heads(plan,accounting);count=min(workers,len(heads));groups=[[] for _ in range(count)]
    # Balance output-weighted traversal cost while keeping every head whole.
    # Global tie allocation is monotone by head, so arbitrary group membership
    # remains correct when each worker's own heads are sorted lexicographically.
    heap=[(0,i) for i in range(count)];heapq.heapify(heap)
    for h in sorted(heads,key=lambda h:(-h['selected']*(h['info']['depth']+1),h['head'])):
        load,i=heapq.heappop(heap);groups[i].append(h)
        heapq.heappush(heap,(load+h['selected']*(h['info']['depth']+1),i))
    model,exe=prepare_native(plan.language,cc);prepared=time.perf_counter()

    def run_group(group):
        begin=time.perf_counter();group=sorted(group,key=lambda h:h['head'])
        wanted=sum(h['selected'] for h in group);below=sum(h['below'] for h in group)
        lines=[f'{plan.length} {accounting["cutoff_cost"]} {wanted} {len(group)}']
        lines.extend(f'{h["head"]} {h["info"]["depth"]} {h["info"]["offset"]} {int(h["info"]["singleton"])}' for h in group)
        data='\n'.join(lines)+'\n';key=hashlib.sha256(data.encode('ascii')).hexdigest()
        request=HERE/'rank_cache'/('parallel_'+key+'.request');_atomic_text(request,data)
        native_start=time.perf_counter()
        process=subprocess.run([str(exe),str(model),str(request)],capture_output=True,text=True,check=True)
        native_seconds=time.perf_counter()-native_start;items=[]
        for line in process.stdout.splitlines():
            score,head,prefix,full=line.split('\t');items.append((int(score),head,prefix,full))
        assert len(items)==wanted
        assert sum(i[0]<accounting['cutoff_cost'] for i in items)==below
        assert co.Counter(i[1] for i in items)=={h['head']:h['selected'] for h in group}
        assert all(i[0]<=accounting['cutoff_cost'] and len(i[3])==plan.length-4 for i in items)
        return items,dict(heads=len(group),selected=wanted,below_cutoff=below,boundary_ties=wanted-below,
                          native_seconds=native_seconds,total_seconds=time.perf_counter()-begin)

    with ThreadPoolExecutor(max_workers=count,thread_name_prefix='kamui-prefix') as executor:
        outputs=list(executor.map(run_group,groups))
    items=[item for group,stats in outputs for item in group];items.sort()
    assert len(items)==accounting['selected'] and len(set(items))==len(items)
    assert sum(i[0]<accounting['cutoff_cost'] for i in items)==accounting['below_cutoff']
    accounting.update(distinct_prefixes=len({i[3] for i in items}),enumerator='native_parallel',
                      workers_requested=workers,workers_used=count,worker_jobs=[stats for group,stats in outputs],
                      preparation_seconds=prepared-started,enumeration_and_merge_seconds=time.perf_counter()-prepared,
                      parallel_total_seconds=time.perf_counter()-started,
                      partition='disjoint head groups; one global cutoff and globally allocated boundary ties')
    return items,accounting
