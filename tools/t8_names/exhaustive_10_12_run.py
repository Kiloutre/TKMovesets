"""Complete search of T8 move names of length 10, 11 and 12 within a character-model cost limit (exhaustive_10_12.c).

No prefix, no length and no owner is supplied: every hash is searched at each of the three lengths, and for a
length every name over the 63 characters whose 4-gram cost is within the limit is hashed and looked up.

Three settings, each with its own character model:

  holdout      vocabulary = verified T8 names minus the evaluation names of exhaustive_10_12_cases.json,
               the owner-accepted names, the public dictionary's extra names. Targets = the sample
               (evaluation and development names of all three lengths).
  production   vocabulary = everything known. Targets = the move hashes with no accepted name.
  blind        vocabulary = verified T8 names minus the evaluation names, and nothing else (no accepted name,
               no dictionary name: the state before any name of a raw unresolved hash was known).
               Targets = the hashes of all accepted names, and the development hashes of the sample, so
               that the judge of the blind test is fitted in that same state.

Every target is searched as itself, under the four frozen XOR masks and under 28 extra XOR masks (the work
does not depend on the number of targets).

  python exhaustive_10_12_run.py cases
  python exhaustive_10_12_run.py search <holdout|production|blind> <length>
"""
import t8names_paths as _P
import collections as co
import gzip
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time

import numpy as np

from name_judge_data import HERE, MASKS, bucket, load_all_overlays, load_labels, load_rows, load_t8_rows, load_targets
from name_judge_search import extra_masks
import short_exhaustive_run as SR

CC = _P.CC
RUNS = HERE/'exhaustive_10_12_runs'
CASES = HERE/'exhaustive_10_12_cases.json'
SALT = 'exhaustive-10-12-sample-v1'
EVAL_RATE = 60; DEVLIST_RATE = 100         # per mille; the second one of what is left after the first
LENGTHS = (10, 11, 12)
LIMIT = 448                               # eighth-bits: 56 bits
EXTRA = 28
WORKERS = int(os.environ.get('EXHAUSTIVE_WORKERS', '6'))
ALPHABET = SR.ALPHABET
NL = chr(10); TAB = chr(9)


def all_masks():
    return list(MASKS)+extra_masks(EXTRA)


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
        if not SR.native_name(name) or len(name) not in LENGTHS or len(x['source_hashes']) != 1 or len(hash_names[x['source_hashes'][0]]) != 1:
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


def setting(name):
    """(vocabulary names, targets [{hash, name or None}], description)."""
    verified = {r['move_name'] for r in load_t8_rows() if r['move_status'] == 'verified' and r['move_name'] and SR.native_name(r['move_name'])}
    overlay = load_all_overlays(); accepted = {n for n in overlay.values() if SR.native_name(n)}
    if name == 'holdout':
        cases = load_cases()['cases']; hidden = {c['name'] for c in cases if c['split'] == 'eval'}
        vocabulary = (verified | accepted | SR.dictionary_extra())-hidden
        targets = [dict(hash=c['hash'], name=c['name']) for c in cases if c['split'] in ('eval', 'devlist')]
        return vocabulary, targets, dict(vocabulary_names=len(vocabulary), hidden_evaluation_names=len(hidden))
    if name == 'production':
        vocabulary = verified | accepted | SR.dictionary_extra()
        targets = [dict(hash=t['hash'], name=None) for t in load_targets('move') if t['hash'] not in overlay]
        return vocabulary, targets, dict(vocabulary_names=len(vocabulary), accepted_names_visible=len(accepted))
    if name == 'blind':
        cases = load_cases()['cases']; hidden = {c['name'] for c in cases if c['split'] == 'eval'}
        vocabulary = verified-hidden
        targets = [dict(hash=c['hash'], name=c['name']) for c in cases if c['split'] == 'devlist']
        targets += [dict(hash='0x%08X' % int(h, 16), name=n, accepted=True) for h, n in sorted(overlay.items())]
        return vocabulary, targets, dict(vocabulary_names=len(vocabulary), hidden_accepted_names=len(overlay), dictionary_names_visible=0, hidden_evaluation_names=len(hidden))
    raise ValueError(name)


FOLDS = 16


def cv_costs(name):
    """{development name: cost under a character model that does not hold it}. The search model of a setting holds
    the development names, so their own cost there is the cost of a memorised name. Here the non-evaluation
    names of the sample are hidden one sixteenth at a time (the evaluation hides 6% of the names at once)."""
    path = RUNS/name/'cv_costs.json'
    if path.exists():
        return json.loads(path.read_text(encoding='utf-8'))
    vocabulary = setting(name)[0]; names = sorted(c['name'] for c in load_cases()['cases'] if c['split'] != 'eval'); out = {}
    for fold in range(FOLDS):
        held = {x for x in names if bucket(x, SALT+':cv', FOLDS) == fold}; table = SR.build_lm(vocabulary-held)
        for x in sorted(held):
            out[x] = int(sum(SR.name_costs(table, x)))
    path.write_text(json.dumps(out, sort_keys=True)+NL, encoding='utf-8')
    return out


def binary():
    source = _P.SRC/'exhaustive_10_12.c'
    key = hashlib.sha256(source.read_bytes()+(_P.SRC/'kamui_search.c').read_bytes()).hexdigest()[:16]
    exe = RUNS/'bin'/('exhaustive_10_12_%s.exe' % key)
    if not exe.exists():
        exe.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([CC, '-O3', '-march=native', '-o', str(exe), str(source)], check=True)
    return exe


def search(name, n, limit=LIMIT, workers=WORKERS):
    from concurrent.futures import ThreadPoolExecutor
    from recover import digest
    started = time.perf_counter(); directory = RUNS/name; directory.mkdir(parents=True, exist_ok=True)
    out_path = directory/('lists_n%d.json.gz' % n)
    if out_path.exists():
        print('exists', out_path, flush=True); return
    vocabulary, targets, description = setting(name)
    lm_path = directory/'lm.bin'; table = SR.build_lm(vocabulary)
    if lm_path.exists():
        assert np.array_equal(np.fromfile(lm_path, dtype=np.uint16), table.reshape(-1)), 'the character model of this setting changed between lengths'
    else:
        table.tofile(lm_path)
    # with the fourth command-line word "merged", the open hashes are looked up in the same walk (the work does not depend on the number of hashes)
    also = setting('production')[1] if (len(sys.argv) > 4 and sys.argv[4] == 'merged' and name == 'holdout') else []
    masks = all_masks(); wanted = sorted({int(t['hash'], 16) ^ m for t in targets+also for m in masks})
    tf = directory/('targets_n%d.txt' % n); tf.write_text(''.join('%08X' % h+NL for h in wanted), encoding='ascii'); exe = binary()

    # one job per first character; a first character that starts at least 2% of the names is split by second character
    share = 2.0**(-table[0, 0, 0, 1:].astype(np.float64)/SR.QUANTUM)
    jobs = []
    for first in range(len(ALPHABET)):
        if share[first] >= 0.02:
            jobs += [(first, second) for second in range(len(ALPHABET))]
        else:
            jobs.append((first, None))

    def job(item):
        first, second = item
        of = directory/('hits_n%d_%02d_%s.txt' % (n, first, 'all' if second is None else '%02d' % second)); last = None
        for attempt in range(5):
            try:
                with of.open('wb') as handle:
                    run = subprocess.run([str(exe), str(lm_path), ALPHABET, str(tf), str(n), str(limit), str(first)]+([] if second is None else [str(second)]),
                                         stdout=handle, stderr=subprocess.PIPE, check=True)
                stats = json.loads(run.stderr.decode()); lines = of.read_text(encoding='ascii').splitlines()
                assert len(lines) == stats['hits'], 'truncated output'
                break
            except (OSError, MemoryError, subprocess.CalledProcessError, AssertionError, ValueError) as error:
                last = error; print('retry', n, item, attempt+1, repr(error)[:200], flush=True); time.sleep(20*(attempt+1))
        else:
            raise RuntimeError('search job failed: %r' % (last,))
        of.unlink()
        return stats, lines

    found = co.defaultdict(list); within = 0; seconds = 0.0
    with ThreadPoolExecutor(workers) as pool:
        for stats, lines in pool.map(job, jobs):
            within += stats['names_within_limit']; seconds += stats['seconds']
            for line in lines:
                h, candidate, cost = line.split(TAB); h = int(h, 16)
                assert digest(candidate) == h and len(candidate) == n
                found[h].append([candidate, int(cost)])
    tf.unlink()
    truth = {}
    for t in targets:
        if t['name'] and len(t['name']) == n:
            first, rest = SR.name_costs(table, t['name']); inside = first+rest <= limit
            truth[t['hash']] = dict(name=t['name'], cost=first+rest, inside=bool(inside))
            if inside:
                assert any(c == t['name'] for c, k in found.get(int(t['hash'], 16), [])), ('a name within the limit is missing', t['name'])
    def save(path, setting_name, chosen, about, known):
        document = dict(setting=setting_name, length=n, limit=limit, limit_bits=limit/8, description=about, masks=['%08X' % m for m in masks], targets=len(chosen),
                        names_of_this_length_per_hash=len(ALPHABET)**n/2**32, names_within_limit=within, chance_names_per_hash_within_limit=within/2**32,
                        processor_seconds=seconds, wall_seconds=time.perf_counter()-started, truth=known,
                        cases=[dict(hash=t['hash'], lists=[sorted(found.get(int(t['hash'], 16) ^ m, [])) for m in masks]) for t in chosen])
        temporary = path.with_suffix('.tmp')
        temporary.write_bytes(gzip.compress(json.dumps(document, separators=(',', ':')).encode(), compresslevel=4, mtime=0)); temporary.replace(path)
        return document

    if also:
        # the open hashes were looked up in the same walk: their lists come from this setting's character model
        other = RUNS/'production'/('lists_n%d.json.gz' % n); other.parent.mkdir(parents=True, exist_ok=True)
        save(other, 'production', also, dict(description, character_model='the model of the %s setting (one walk for both sets of hashes)' % name), {})
    document = save(out_path, name, targets, description, truth)
    sizes = [len(l) for c in document['cases'] for l in c['lists'][1:]]
    print(name, 'length', n, 'names within limit %.4g' % within, 'expected per hash %.2f' % (within/2**32), 'mean decoy list %.2f' % (sum(sizes)/max(1, len(sizes))),
          'true names of this length inside', sum(v['inside'] for v in truth.values()), '/', len(truth), 'wall %.0f s' % (time.perf_counter()-started), flush=True)


def load_lists(name, n):
    return json.loads(gzip.decompress((RUNS/name/('lists_n%d.json.gz' % n)).read_bytes()))


if __name__ == '__main__':
    if sys.argv[1] == 'cases':
        document = build_cases(); text = json.dumps(document, indent=1, sort_keys=True)+NL
        if CASES.exists():
            assert CASES.read_text(encoding='utf-8') == text, 'frozen sample differs'
        else:
            CASES.write_text(text, encoding='utf-8')
        print(document['counts'], document['by_length'])
    elif sys.argv[1] == 'search':
        search(sys.argv[2], int(sys.argv[3]))
