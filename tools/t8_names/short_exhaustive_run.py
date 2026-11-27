"""Complete search of T8 move names of length 5..9 (short_exhaustive.c), with a known-answer sample.

No prefix, no length and no owner is supplied to the search: for every target hash every name of each
length 5..9 over the 63-character alphabet with that hash is found, and a character 4-gram model of
whole names keeps the plausible ones (all of them for lengths 5 and 6).

  python short_exhaustive_run.py cases        frozen sample (short_exhaustive_cases.json), written once
  python short_exhaustive_run.py holdout      model without the evaluation names, lists for the sample
  python short_exhaustive_run.py production   model with every known name, lists for the open hashes

Vocabulary of the 4-gram model: verified T8 move names of every length, the owner-accepted names of all
accepted_moves_overlay*.csv files, and the public dictionary's names whose hash is not in our T8 export
(extra vocabulary; flagged in the output). In the holdout run the evaluation names are removed.

Every target is searched as itself, under the four frozen XOR masks and under four extra XOR masks.
Nothing is accepted here.
"""
import t8names_paths as _P
import collections as co
import csv
import gzip
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time

import numpy as np

from name_judge_data import HERE, MASKS, bucket, load_all_overlays, load_labels, load_rows, load_t8_rows, load_targets
from name_judge_search import extra_masks

CC = _P.CC
RUNS = HERE/'short_exhaustive_runs'
CASES = HERE/'short_exhaustive_cases.json'
SALT = 'short-exhaustive-sample-v1'
EVAL_RATE = 60; DEVLIST_RATE = 120          # per mille; the second one of what is left after the first
LENGTHS = (5, 6, 7, 8, 9)
KEEP = {5: None, 6: None, 7: 300, 8: 300, 9: 300}      # chance matches kept per hash and length (None = all of them)
EXTRA = 4
QUANTUM = 8
ALPHABET = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz'
CURVE = (10, 30, 100, 300, 1000, 3000, 10000)
ENDING_SHARE = {9: 0.002}                   # length 9: share of random endings (everything after the first four characters) that is kept
NL = chr(10); TAB = chr(9)


def all_masks():
    return list(MASKS)+extra_masks(EXTRA)


def native_name(name):
    return bool(name) and all(c in ALPHABET for c in name)


def build_cases():
    labels = [x for x in load_labels() if x['game'] == 'T8' and x['namespace'] == 'move' and x['status'] == 'verified']
    rows = [r for r in load_rows() if r['game'] == 'T8']
    by_hash = co.defaultdict(list)
    for r in rows:
        if r['move_hash']:
            by_hash[r['move_hash']].append(r)
    hash_names = co.defaultdict(set)
    for x in labels:
        for h in x['source_hashes']:
            hash_names[h].add(x['name'])
    cases = []
    for x in sorted(labels, key=lambda x: x['name']):
        name = x['name']
        if not native_name(name) or not 5 <= len(name) <= 9 or len(x['source_hashes']) != 1 or len(hash_names[x['source_hashes'][0]]) != 1:
            continue
        h = x['source_hashes'][0]
        first = bucket(name, SALT+':eval', 1000); second = bucket(name, SALT+':dev', 1000); third = bucket(name, SALT+':role', 2)
        split = 'eval' if first < EVAL_RATE else ('devlist' if second < DEVLIST_RATE else 'devpos')
        located = sorted({(r['code'], int(r['id'])) for r in by_hash[h]})
        cases.append(dict(name=name, hash=h, length=len(name), split=split, role=('train' if third == 0 else 'calibration') if split == 'devlist' else split,
                          codes=sorted({c for c, i in located}), rows=[list(p) for p in located]))
    return dict(schema_version=1, salt=SALT, eval_per_mille=EVAL_RATE, devlist_per_mille=DEVLIST_RATE,
                counts=dict(co.Counter(c['role'] for c in cases)), by_length={str(n): dict(co.Counter(c['role'] for c in cases if c['length'] == n)) for n in LENGTHS},
                cases=cases)


def load_cases():
    return json.loads(CASES.read_text(encoding='utf-8'))


def dictionary_extra():
    """Names of the public dictionary whose hash is in no row of our T8 export (vocabulary only)."""
    out = set()
    if not (HERE/'public_dictionary_verified.csv').exists():
        # new layout: merge_t8_names.py writes the re-hashing public names that no row uses
        extra = _P.WORKSPACE/'merged'/'extra_names.json'
        return {n for n in (json.loads(extra.read_text(encoding='utf-8')).values() if extra.exists() else ()) if native_name(n)}
    with (HERE/'public_dictionary_verified.csv').open(encoding='utf-8', newline='') as f:
        for r in csv.DictReader(f):
            if not r.get('t8_move') and not r.get('t8_anim') and native_name(r['name']) and r.get('rehash_verified') == 'yes':
                out.add(r['name'])
    return out


def vocabulary(hidden=()):
    """(names of our lists, extra dictionary names). Hidden names are in neither."""
    hidden = set(hidden); overlay = load_all_overlays()
    ours = {r['move_name'] for r in load_t8_rows() if r['move_status'] == 'verified' and r['move_name']} | set(overlay.values())
    ours = {n for n in ours if native_name(n) and n not in hidden}
    extra = {n for n in dictionary_extra() if n not in hidden and n not in ours}
    return ours, extra


def build_lm(names):
    """Interpolated character 4-gram model of whole names (Witten-Bell weights), costs in eighth-bits.
    table[c1, c2, c3, x]: c = 0 is the start padding, x = 0 is the end of the name, 1..63 are the characters."""
    size = len(ALPHABET)+1; code = {c: i+1 for i, c in enumerate(ALPHABET)}
    counts = np.zeros((size, size, size, size), dtype=np.float64)
    for name in sorted(set(names)):
        seq = [0, 0, 0]+[code[c] for c in name]+[0]
        for i in range(3, len(seq)):
            counts[seq[i-3], seq[i-2], seq[i-1], seq[i]] += 1
    def witten_bell(c, lower):
        total = c.sum(axis=-1, keepdims=True); kinds = (c > 0).sum(axis=-1, keepdims=True)
        weight = np.where(total > 0, total/np.maximum(total+kinds, 1e-12), 0.0)
        return weight*c/np.maximum(total, 1e-12)+(1-weight)*lower
    c3 = counts.sum(axis=0); c2 = c3.sum(axis=0); c1 = c2.sum(axis=0)
    p1 = (c1+0.5)/(c1.sum()+0.5*size)
    p2 = witten_bell(c2, p1[None, :]); p3 = witten_bell(c3, p2[None, :, :]); p4 = witten_bell(counts, p3[None, :, :, :])
    cost = np.rint(-QUANTUM*np.log2(np.maximum(p4, 1e-300)))
    return np.minimum(cost, 60000).astype(np.uint16)


def name_costs(table, name):
    """(cost of the first four characters, cost of everything after them, the end included)."""
    code = {c: i+1 for i, c in enumerate(ALPHABET)}; a = b = c = 0; first = 0; rest = 0
    for position, ch in enumerate(name):
        x = code[ch]; value = int(table[a, b, c, x]); a, b, c = b, c, x
        if position < 4:
            first += value
        else:
            rest += value
    return first, rest+int(table[a, b, c, 0])


def name_cost(table, name):
    return sum(name_costs(table, name))


def random_costs(table, n, samples=20_000_000, seed=11):
    """Costs of uniformly random names of length n (the distribution of chance matches)."""
    rng = np.random.default_rng(seed+n); out = []
    for start in range(0, samples, 2_000_000):
        m = min(2_000_000, samples-start); codes = rng.integers(1, len(ALPHABET)+1, size=(m, n), dtype=np.int64)
        padded = np.concatenate([np.zeros((m, 3), dtype=np.int64), codes, np.zeros((m, 1), dtype=np.int64)], axis=1)
        first = np.zeros(m, dtype=np.int64); rest = np.zeros(m, dtype=np.int64)
        for i in range(3, n+4):
            value = table[padded[:, i-3], padded[:, i-2], padded[:, i-1], padded[:, i]]
            if i < 7:
                first += value
            else:
                rest += value
        out.append(np.stack([first, rest], axis=1).astype(np.int32))
    return np.concatenate(out)


def cutoffs_for(table):
    """Per length: the cost cutoff that keeps KEEP[n] chance matches per hash, and the kept count at other cutoffs."""
    out = {}
    for n in LENGTHS:
        per_hash = len(ALPHABET)**n/2**32
        if KEEP[n] is None:
            out[n] = dict(cutoff=-1, expected_chance_matches_per_hash=per_hash, all_matches_per_hash=per_hash, curve={}); continue
        both = random_costs(table, n); share = KEEP[n]/per_hash
        if n in ENDING_SHARE:
            # two limits instead of one: the ending limit prunes the enumeration exactly, the start limit sets the number kept
            ending = int(np.sort(both[:, 1])[int(ENDING_SHARE[n]*len(both))]); subset = np.sort(both[both[:, 1] <= ending, 0])
            wanted = int(share*len(both)); start = int(subset[min(len(subset)-1, max(0, wanted-1))])
            kept = float(((both[:, 1] <= ending) & (both[:, 0] <= start)).sum())/len(both)*per_hash
            curve = {}
            for k in CURVE:
                position = int(k/per_hash*len(both))-1
                if 0 <= position < len(subset):
                    curve[str(k)] = int(subset[position])        # the start limit that would keep k chance matches per hash
            out[n] = dict(cutoff=-1, start_cutoff=start, ending_cutoff=ending, ending_share=ENDING_SHARE[n], expected_chance_matches_per_hash=kept,
                          all_matches_per_hash=per_hash, curve=curve); continue
        costs = np.sort(both.sum(axis=1))
        cutoff = int(costs[min(len(costs)-1, max(0, int(share*len(costs))-1))])
        kept = float(np.searchsorted(costs, cutoff, side='right'))/len(costs)*per_hash
        curve = {}
        for k in CURVE:
            s = k/per_hash
            if s < 1:
                curve[str(k)] = int(costs[min(len(costs)-1, max(0, int(s*len(costs))-1))])
        out[n] = dict(cutoff=cutoff, expected_chance_matches_per_hash=kept, all_matches_per_hash=per_hash, curve=curve)
    return out


def binary():
    source = _P.SRC/'short_exhaustive.c'
    key = hashlib.sha256(source.read_bytes()+(_P.SRC/'kamui_search.c').read_bytes()).hexdigest()[:16]
    exe = RUNS/'bin'/('short_exhaustive_%s.exe' % key)
    if not exe.exists():
        exe.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([CC, '-O3', '-march=native', '-o', str(exe), str(source)], check=True)
    return exe


def search(targets, lm_path, cutoffs, directory, workers=3, chunk=1500):
    """{hash: [[name, cost], ...]} for every target, all lengths. Complete within the cutoffs."""
    from concurrent.futures import ThreadPoolExecutor
    from recover import digest
    exe = binary(); directory = Path(directory); directory.mkdir(parents=True, exist_ok=True)
    targets = sorted(set(targets)); jobs = []
    for n in LENGTHS:
        for start in range(0, len(targets), chunk):
            jobs.append((n, start, targets[start:start+chunk]))

    def job(item):
        n, start, part = item; tag = 'n%d_%d' % (n, start)
        tf = directory/('targets_%s.txt' % tag); of = directory/('hits_%s.txt' % tag)
        tf.write_text(''.join('%08X' % h+NL for h in part), encoding='ascii'); last = None
        for attempt in range(5):
            try:
                with of.open('wb') as handle:
                    run = subprocess.run([str(exe), str(lm_path), ALPHABET, str(tf), str(n)]+[str(v) for v in cutoffs[n]], stdout=handle, stderr=subprocess.PIPE, check=True)
                stats = json.loads(run.stderr.decode()); lines = of.read_text(encoding='ascii').splitlines()
                assert len(lines) == stats['hits'], 'truncated output'
                break
            except (OSError, MemoryError, subprocess.CalledProcessError, AssertionError, ValueError) as error:
                last = error; print('retry', tag, attempt+1, repr(error)[:200], flush=True); time.sleep(20*(attempt+1))
        else:
            raise RuntimeError('search job failed: %r' % (last,))
        tf.unlink(); of.unlink()
        return n, stats, lines

    found = co.defaultdict(list); statistics = co.defaultdict(lambda: co.Counter()); started = time.perf_counter()
    with ThreadPoolExecutor(workers) as pool:
        for n, stats, lines in pool.map(job, sorted(jobs, key=lambda j: (-j[0], j[1]))):
            for key in ('enumerated', 'derived', 'alphabet_level_matches', 'hits'):
                statistics[n][key] += stats[key]
            statistics[n]['seconds'] += stats['seconds']; statistics[n]['targets'] += stats['targets']
            for line in lines:
                h, name, cost = line.split(TAB); h = int(h, 16)
                assert digest(name) == h and len(name) == n
                found[h].append([name, int(cost)])
    return found, {str(n): dict(v) for n, v in statistics.items()}, time.perf_counter()-started


def prepare(directory, hidden):
    directory.mkdir(parents=True, exist_ok=True)
    ours, extra = vocabulary(hidden); table = build_lm(ours | extra)
    lm_path = directory/'lm.bin'; table.tofile(lm_path)
    setup = dict(alphabet=ALPHABET, keep=KEEP, vocabulary_ours=len(ours), vocabulary_dictionary_extra=len(extra), hidden=len(hidden),
                 masks=['%08X' % m for m in all_masks()], lengths={str(n): v for n, v in cutoffs_for(table).items()})
    return table, lm_path, setup


def limits_of(setup):
    """{n: command-line limits of the native tool} and a test 'is this name inside the kept region'."""
    limits = {}
    for n in LENGTHS:
        d = setup['lengths'][str(n)]
        limits[n] = (d['cutoff'], d['start_cutoff'], d['ending_cutoff']) if 'start_cutoff' in d else (d['cutoff'],)
    def inside(table, name):
        d = setup['lengths'][str(len(name))]; first, rest = name_costs(table, name)
        if 'start_cutoff' in d:
            return bool(first <= d['start_cutoff'] and rest <= d['ending_cutoff'])
        return bool(d['cutoff'] < 0 or first+rest <= d['cutoff'])
    return limits, inside


def save(path, document):
    path = Path(path); temporary = path.with_suffix('.tmp')
    temporary.write_bytes(gzip.compress(json.dumps(document, separators=(',', ':')).encode(), compresslevel=4, mtime=0)); temporary.replace(path)


def load(path):
    return json.loads(gzip.decompress(Path(path).read_bytes()))


def holdout(workers):
    document = load_cases(); directory = RUNS/'holdout'
    hidden = {c['name'] for c in document['cases'] if c['split'] == 'eval'}
    table, lm_path, setup = prepare(directory, hidden)
    cutoffs, inside = limits_of(setup)
    listed = [c for c in document['cases'] if c['split'] in ('eval', 'devlist')]
    truth = {}
    for c in document['cases']:
        first, rest = name_costs(table, c['name']); cost = first+rest; n = c['length']; d = setup['lengths'][str(n)]
        if 'start_cutoff' in d:
            inside_at = {k: bool(first <= v and rest <= d['ending_cutoff']) for k, v in d['curve'].items()}
        else:
            inside_at = {k: bool(cost <= v) for k, v in d['curve'].items()}
        truth[c['hash']] = dict(name=c['name'], cost=cost, cost_of_the_first_four=first, inside=inside(table, c['name']), inside_at=inside_at)
    setup['truth'] = truth; (directory/'setup.json').write_text(json.dumps(setup, indent=1)+NL, encoding='utf-8')
    print({n: (cutoffs[n], round(setup['lengths'][str(n)]['expected_chance_matches_per_hash'], 1)) for n in LENGTHS}, flush=True)
    masks = all_masks(); targets = [int(c['hash'], 16) ^ m for c in listed for m in masks]
    found, statistics, seconds = search(targets, lm_path, cutoffs, directory, workers)
    out = dict(masks=setup['masks'], statistics=statistics, seconds=seconds,
               cases=[dict(hash=c['hash'], lists=[sorted(found.get(int(c['hash'], 16) ^ m, [])) for m in masks]) for c in listed])
    save(directory/'lists.json.gz', out)
    for c in listed:
        if truth[c['hash']]['inside']:
            assert any(name == c['name'] for name, cost in found.get(int(c['hash'], 16), [])), ('true name missing from a complete list', c['name'])
    print(json.dumps(statistics), 'seconds %.0f' % seconds, flush=True)


def production(workers):
    directory = RUNS/'production'; overlay = load_all_overlays()
    targets = [t for t in load_targets('move') if t['hash'] not in overlay]
    table, lm_path, setup = prepare(directory, ())
    cutoffs, inside = limits_of(setup)
    setup['open_hashes'] = len(targets); (directory/'setup.json').write_text(json.dumps(setup, indent=1)+NL, encoding='utf-8')
    masks = all_masks(); wanted = [int(t['hash'], 16) ^ m for t in targets for m in masks]
    found, statistics, seconds = search(wanted, lm_path, cutoffs, directory, workers)
    out = dict(masks=setup['masks'], statistics=statistics, seconds=seconds,
               cases=[dict(hash=t['hash'], lists=[sorted(found.get(int(t['hash'], 16) ^ m, [])) for m in masks]) for t in targets])
    save(directory/'lists.json.gz', out)
    print(json.dumps(statistics), 'seconds %.0f' % seconds, flush=True)


if __name__ == '__main__':
    command = sys.argv[1]; workers = int(sys.argv[2]) if len(sys.argv) > 2 else 3
    if command == 'cases':
        document = build_cases(); text = json.dumps(document, indent=1, sort_keys=True)+NL
        if CASES.exists():
            assert CASES.read_text(encoding='utf-8') == text, 'frozen sample differs'
        else:
            CASES.write_text(text, encoding='utf-8')
        print(document['counts'], document['by_length'])
    elif command == 'holdout':
        holdout(workers)
    elif command == 'production':
        production(workers)
