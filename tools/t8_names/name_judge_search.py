"""Wide prefix search for many target hashes at once (name-judge round).

The existing engines are imported unchanged: MoveModel / SubwordGrammar /
LengthPrior for the plan, parallel_prefixes.select_parallel for the ranked
prefixes, tail_solver.c for the algebraic last-four-byte solve.

The only new thing is the batching. A prefix list depends on the fitted model,
the owner codes, the length and the budget, never on the target hash. So one
list is built per (model, codes, budget) and then solved against every hash
that shares it: the real target, the four frozen XOR decoys and any number of
extra XOR decoys. One total budget is split over lengths 13..24 by the frozen
LengthPrior, exactly as typed prefix searches did before.

Every returned name is re-hashed. Nothing is accepted here.
"""
import t8names_paths as _P
import collections as co
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import time

from name_judge_data import HERE, MASKS

CC = _P.CC
RUNS = HERE/'name_judge_runs'
LENGTHS = tuple(range(13, 25))
EXTRA_SALT = 'name-judge-extra-decoy-v1'


def extra_masks(count):
    """Deterministic extra XOR masks, disjoint from the four frozen ones and from 0."""
    out = []; i = 0
    while len(out) < count:
        value = int.from_bytes(hashlib.sha256((EXTRA_SALT+':'+str(i)).encode()).digest()[:4], 'big'); i += 1
        if value and value not in MASKS and value not in out:
            out.append(value)
    return out


def fit(rows, names):
    """rows: dicts with move_name and code (visible verified T8 rows); names: the visible name set."""
    from move_pipeline import MoveModel
    from length_allocation import LengthPrior
    rows = [r for r in rows if r['move_name'] in names]
    model = MoveModel([dict(label=r['move_name'], code=r['code']) for r in rows], names, 'move_t8')
    language = model.language().build_histograms()
    prior = LengthPrior(rows, names, 'move_name')
    return model, language, prior


def lane_plans(model, language, prior, codes, budget, lengths=LENGTHS):
    codes = sorted(set(codes)); allocation = prior.allocate(budget, codes, lengths)
    return [(n, allocation[n]) for n in sorted(allocation) if allocation[n] > 0]


RETRY = (MemoryError, OSError, subprocess.CalledProcessError, AssertionError, IndexError, ValueError)


def _retry(function, label, retries=8):
    """Memory on this machine is shared with other sessions; a starved call is repeated, never trusted."""
    for attempt in range(retries):
        try:
            return function()
        except RETRY as error:
            print('retry', label, attempt+1, repr(error)[:160], flush=True)
            time.sleep(30*(attempt+1))
    raise RuntimeError('failed after retries: '+label)


def run_group(model, language, prior, codes, budget, cases, extra, directory, workers=1, lane_threads=4, lengths=LENGTHS):
    """One code tuple: ranked prefixes per length under ONE total budget, solved for every case.

    cases: [{'key': str, 'hash': int, 'name': str or None}]. Returns a JSON-able document.
    No target hash takes part in building a lane; hashes only reach the algebraic solver.
    The solver's own hit count is compared with the lines read, so a truncated pipe cannot
    silently drop matches.
    """
    from concurrent.futures import ThreadPoolExecutor
    from native_cache import compiled
    from parallel_prefixes import select_parallel
    from recover import digest
    started = time.perf_counter(); directory = Path(directory); directory.mkdir(parents=True, exist_ok=True)
    codes = sorted(set(codes)); masks = list(MASKS)+extra_masks(extra)
    wanted = {}
    for case in cases:
        for index, mask in enumerate(masks):
            wanted.setdefault(case['hash'] ^ mask, []).append((case['key'], index))
    hashes = sorted(wanted); assert 0 < len(hashes) < 65535
    import name_judge_short
    long_exe = compiled(_P.SRC/'tail_solver.c', HERE/'rank_cache', CC, dependencies=(_P.SRC/'kamui_search.c',))
    short_exe = name_judge_short.solver() if min(lengths) < 13 else None
    tag = hashlib.sha256(json.dumps([codes, budget, hashes[:4], len(hashes)]).encode()).hexdigest()[:12]
    targets = directory/('targets_%s.txt' % tag)
    targets.write_text(''.join('%08X 0\n' % h for h in hashes), encoding='ascii')
    plan_list = lane_plans(model, language, prior, codes, budget, lengths)
    plans = {n: _retry(lambda n=n: name_judge_short.plan(model, language, codes, n), 'plan %d' % n) for n, quota in plan_list}

    def build(item):
        n, quota = item
        def once():
            if n < 13:      # the native enumerator refuses lengths below 13; the Python enumerator of the same plan is exact and cheap at this depth
                items, accounting = plans[n].select(quota)
            else:
                items, accounting = select_parallel(plans[n], quota, workers, CC)
            assert len(items) == accounting['selected']
            return sorted({entry[3] for entry in items})
        return n, _retry(once, 'lane %s %d' % (codes[:2], n))
    begin = time.perf_counter()
    with ThreadPoolExecutor(lane_threads) as pool:
        prefix_lists = dict(pool.map(build, plan_list))
    generation = time.perf_counter()-begin
    found = co.defaultdict(list); alphabet = co.Counter(); truth = {}; quotas = {}; total_prefixes = 0; space = 0
    for n, quota in plan_list:
        plan = plans[n]; prefixes = prefix_lists.pop(n); prefixset = set(prefixes)
        pf = directory/('prefixes_%s_n%d.txt' % (tag, n))
        pf.write_text(''.join(p+"\n" for p in prefixes), encoding='ascii')

        def once(path=None):
            run = subprocess.run([str(long_exe if n >= 13 else short_exe), str(targets), str(path or pf), plan.alphabet], capture_output=True, text=True, check=True)
            lines = run.stdout.splitlines(); stats = json.loads(run.stderr)
            assert stats['hits'] == len(lines) and stats['targets'] == len(hashes), 'truncated solver output'
            return lines
        if n == 10 and len(prefixes) > 200:
            # length 10 costs |alphabet|^2 solves per (prefix, target): split the prefixes over the lane threads
            chunks = [prefixes[i::lane_threads] for i in range(lane_threads)]; paths = []
            for i, chunk in enumerate(chunks):
                path = directory/('prefixes_%s_n%d_%d.txt' % (tag, n, i)); path.write_text(''.join(q+"\n" for q in chunk), encoding='ascii'); paths.append(path)
            with ThreadPoolExecutor(lane_threads) as pool:
                parts = list(pool.map(lambda path: _retry(lambda: once(path), 'solve %s %d' % (codes[:2], n)), paths))
            lines = [line for part in parts for line in part]
            for path in paths:
                path.unlink()
        else:
            lines = _retry(once, 'solve %s %d' % (codes[:2], n))
        quotas[str(n)] = quota; total_prefixes += len(prefixes); space += plan.represented_counts(prefixes)['grammar']
        for line in lines:
            hh, name = line.split("\t"); h = int(hh, 16)
            if not plan.accepts(name, False):
                continue
            alphabet[h] += 1
            if plan.accepts(name, True):
                assert digest(name) == h and len(name) == n and name[:-4] in prefixset
                found[h].append(name)
        for case in cases:
            name = case.get('name')
            if name and len(name) == n:
                inside = name[:-4] in prefixset
                truth[case['key']] = dict(lane=True, prefix_listed=inside, grammar=bool(inside and plan.accepts(name, True)),
                                          alphabet=bool(inside and plan.accepts(name, False)), quota=quota)
        pf.unlink(); del prefixes, prefixset, lines
    targets.unlink()
    out = []
    for case in cases:
        lists = []; counts = []
        for index, mask in enumerate(masks):
            h = case['hash'] ^ mask
            lists.append(sorted(found.get(h, []))); counts.append(alphabet.get(h, 0))
        entry = dict(key=case['key'], hash='0x%08X' % case['hash'], lists=lists, alphabet_counts=counts)
        if case.get('name'):
            status = truth.get(case['key'], dict(lane=False, prefix_listed=False, grammar=False, alphabet=False, quota=0))
            status['found'] = case['name'] in lists[0]
            assert status['found'] == status['grammar'], (case['name'], status)
            entry['truth'] = status
        out.append(entry)
    return dict(codes=codes, budget=budget, lengths=[int(n) for n in quotas], quotas=quotas, prefixes=total_prefixes,
                represented_grammar_names=space, expected_chance_matches_per_hash=space/2**32,
                masks=['%08X' % m for m in masks], generation_seconds=generation,
                total_seconds=time.perf_counter()-started, cases=out)


def save(path, document):
    path = Path(path); path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(gzip.compress(json.dumps(document, separators=(',', ':')).encode(), compresslevel=4, mtime=0))


def load(path):
    return json.loads(gzip.decompress(Path(path).read_bytes()))
