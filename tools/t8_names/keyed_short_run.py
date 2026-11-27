"""Complete search of move names of length 5 to 10 with the move key, no character model (keyed_short.c).

Every string of the length over the 63 name characters is covered (by algebra: bytes 4..n-1 are enumerated and
bytes 0..3 follow from the hash; for lengths 5 to 7 the roles are swapped). A name with the wanted hash is kept
when Kamui(CODE_name) equals the key of the first move record of the hash; then all records are counted.

Wanted hashes (no length and no prefix is supplied: every hash is searched at every length):

  open            the move hashes with no accepted name
  doubtful label  the hashes of the eleven verified names that no list code reproduces the key of
                  (coordinator, commit 9e52163): they are searched as if they had no name
  known answer    a frozen sample of verified names of length 5 to 10 whose key agrees: the tool gets hash and
                  key only and must return exactly these names

Controls:
  decoys on the hash   every wanted hash XOR each of the four frozen masks, same records: identical pipeline.
                       Lengths 5 to 9 in full; length 10 on 8 of the 63 slices of the string space (the full
                       decoy pass would cost four times the real one).
  decoys on the key    every kept name of the real run is also compared with key ^ mask (free)
  wrong code           every kept name of the real run is also keyed with the code of another list (free)
The number of chance agreements expected is exact: (names with a wanted hash) / 2^32 per comparison.

  python keyed_short_run.py targets          freeze the wanted hashes -> keyed_short_targets.json
  python keyed_short_run.py run <n>          one length, real and decoy runs -> keyed_runs/short/n<n>.json
  python keyed_short_run.py report           -> keyed_short_results.json, keyed_short_names.csv
"""
import t8names_paths as _P
import collections as co
import csv
import hashlib
import json
import os
import subprocess
import sys
import time

import numpy as np

from name_judge_data import HERE, MASKS, bucket, load_all_overlays, load_labels, load_t8_rows, load_targets
import short_exhaustive_run as SR
import move_key as K
from recover import digest

CC = _P.CC
RUNS = HERE/'keyed_runs'/'short'
TARGETS = HERE/'keyed_short_targets.json'
LENGTHS = (5, 6, 7, 8, 9, 10)
SALT = 'keyed-short-known-answers-v1'
PER_LENGTH = 20
DOUBTFUL = ['Bx_Iqrok', 'Cat_ScsGg00_n', 'Cat_ScsGg00_y', 'Fu_Thrmv_bBai', 'Lw_N_way4', 'Ni_Side_LK_LFMist2', 'cAG_1_MB7', 'cGrd_heavyR40_HUZIOK',
            'sDm_KAOR_ZG178', 'sDm_KAOZenRz', 'sDm_tuki_hi_Nsgkou']
DECOY_SLICES_10 = (0, 8, 16, 24, 32, 40, 48, 56)
WORKERS = int(os.environ.get('EXHAUSTIVE_WORKERS', '10'))
ALPHABET = SR.ALPHABET
NL = chr(10); TAB = chr(9)


def build_targets():
    keys = K.load_keys(); overlay = load_all_overlays()
    out = [dict(hash=t['hash'], group='open') for t in load_targets('move') if t['hash'] not in overlay]
    out += [dict(hash='0x%08X' % digest(name), group='doubtful label', label=name) for name in DOUBTFUL]
    labels = [x for x in load_labels() if x['game'] == 'T8' and x['namespace'] == 'move' and x['status'] == 'verified' and len(x['source_hashes']) == 1]
    names_of = co.defaultdict(set)
    for x in labels:
        names_of[x['source_hashes'][0]].add(x['name'])
    for n in LENGTHS:
        pool = []
        for x in sorted(labels, key=lambda x: x['name']):
            name = x['name']; h = int(x['source_hashes'][0], 16)
            if len(name) != n or not SR.native_name(name) or len(names_of[x['source_hashes'][0]]) != 1 or name in DOUBTFUL:
                continue
            ok, total = K.agree(keys, h, name)
            if total and ok == total:
                pool.append((bucket(name, SALT, 1 << 30), name, h))
        for _, name, h in sorted(pool)[:PER_LENGTH]:
            out.append(dict(hash='0x%08X' % h, group='known answer', name=name))
    assert len({t['hash'] for t in out}) == len(out)
    return dict(schema_version=1, salt=SALT, known_answers_per_length=PER_LENGTH, counts=dict(co.Counter(t['group'] for t in out)), targets=out)


def load():
    return json.loads(TARGETS.read_text(encoding='utf-8'))['targets']


def binary():
    source = _P.SRC/'keyed_short.c'
    key = hashlib.sha256(source.read_bytes()+(_P.SRC/'kamui_search.c').read_bytes()).hexdigest()[:16]
    exe = HERE/'keyed_runs'/'bin'/('keyed_short_%s.exe' % key)
    if not exe.exists():
        exe.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([CC, '-O3', '-march=native', '-o', str(exe), str(source)], check=True)
    return exe


def write_targets(path, hashes, keys):
    codes = K.codes_of(keys); other = {c: codes[(i+1) % len(codes)] for i, c in enumerate(codes)}; lines = []
    for value, records in hashes:
        lines.append('%08X %s' % (value, ' '.join('%s:%08X:%s' % (c.upper(), k, other[c].upper()) for c, _, k in records)))
    path.write_text(NL.join(lines)+NL, encoding='ascii')


def run(n):
    from concurrent.futures import ThreadPoolExecutor
    RUNS.mkdir(parents=True, exist_ok=True); out_path = RUNS/('n%d.json' % n)
    if out_path.exists():
        print('exists', out_path); return
    keys = K.load_keys(); targets = load(); exe = binary(); started = time.perf_counter()
    real = [(int(t['hash'], 16), keys[int(t['hash'], 16)]) for t in targets]
    decoy = [(h ^ m, records) for m in MASKS[1:] for h, records in real]
    files = dict(real=RUNS/('targets_real_n%d.txt' % n), decoy=RUNS/('targets_decoy_n%d.txt' % n))
    write_targets(files['real'], real, keys); write_targets(files['decoy'], decoy, keys)
    jobs = [('real', first) for first in range(len(ALPHABET))]
    jobs += [('decoy', first) for first in (range(len(ALPHABET)) if n < 10 else DECOY_SLICES_10)]

    def job(item):
        kind, first = item; last = None
        for attempt in range(4):
            try:
                done = subprocess.run([str(exe), str(files[kind]), ALPHABET, str(n), str(first)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)
                return kind, first, json.loads(done.stderr.decode()), done.stdout.decode('ascii').splitlines()
            except (OSError, MemoryError, subprocess.CalledProcessError, ValueError) as error:
                last = error; print('retry', n, item, attempt+1, repr(error)[:200], flush=True); time.sleep(15*(attempt+1))
        raise RuntimeError('keyed search job failed: %r' % (last,))

    totals = {kind: co.Counter() for kind in files}; hits = {kind: [] for kind in files}
    with ThreadPoolExecutor(WORKERS) as pool:
        for kind, first, stats, lines in pool.map(job, jobs):
            for field in ('strings', 'names_with_a_wanted_hash', 'key', 'mask1', 'mask2', 'mask3', 'mask4', 'wrongcode'):
                totals[kind][field] += stats[field]
            totals[kind]['processor_seconds'] += stats['seconds']; totals[kind]['slices'] += 1
            for line in lines:
                what, value, name, agree, total = line.split(TAB)
                assert len(name) == n
                hits[kind].append(dict(kind=what, hash='0x'+value, name=name, agree=int(agree), records=int(total)))
    document = dict(length=n, targets=len(real), decoy_targets=len(decoy), slices=len(ALPHABET), decoy_slices=totals['decoy']['slices'],
                    strings_of_this_length=len(ALPHABET)**n, totals={k: dict(v) for k, v in totals.items()}, hits=hits, wall_seconds=time.perf_counter()-started)
    out_path.write_text(json.dumps(document, indent=1)+NL, encoding='utf-8')
    for path in files.values():
        path.unlink()
    print('length', n, {k: dict(v) for k, v in totals.items()}, 'wall %.0f s' % document['wall_seconds'], flush=True)
    for h in hits['real']:
        print('   ', h)
    print('    decoy hits:', hits['decoy'])


def report():
    keys = K.load_keys(); targets = load(); by_hash = {t['hash']: t for t in targets}
    lm_path = HERE/'exhaustive_10_12_runs'/'production'/'lm.bin'
    if not lm_path.exists():      # character model of every known name (the one the production search of lengths 10 to 12 writes)
        import exhaustive_10_12_run as EX
        lm_path.parent.mkdir(parents=True, exist_ok=True); SR.build_lm(EX.setting('production')[0]).tofile(lm_path)
    table = np.fromfile(lm_path, dtype=np.uint16).reshape(64, 64, 64, 64)
    lengths = {}; names = []; found_known = {}
    for n in LENGTHS:
        path = RUNS/('n%d.json' % n)
        if not path.exists():
            continue
        d = json.loads(path.read_text(encoding='utf-8')); real = d['totals']['real']; decoy = d['totals']['decoy']
        entry = dict(strings_of_this_length=d['strings_of_this_length'], wanted_hashes=d['targets'],
                     names_with_a_wanted_hash=real['names_with_a_wanted_hash'], chance_agreements_expected_per_comparison=real['names_with_a_wanted_hash']/2**32,
                     names_confirmed_by_the_key=real['key'], confirmed_with_a_masked_key=[real['mask%d' % j] for j in (1, 2, 3, 4)], confirmed_with_another_code=real['wrongcode'],
                     decoy_hashes=d['decoy_targets'], decoy_share_of_the_strings=d['decoy_slices']/d['slices'], decoy_names_with_a_wanted_hash=decoy['names_with_a_wanted_hash'],
                     decoy_names_confirmed_by_the_key=decoy['key'], decoy_chance_agreements_expected=decoy['names_with_a_wanted_hash']/2**32,
                     processor_seconds=real['processor_seconds']+decoy['processor_seconds'], wall_seconds=d['wall_seconds'])
        # the control agreements are chance names: their character-model cost shows what a chance name looks like
        chance = [dict(kind=h['kind'], hash=h['hash'], name=h['name'], character_model_cost_bits=sum(SR.name_costs(table, h['name']))/8) for h in d['hits']['real'] if h['kind'] != 'key']
        chance += [dict(kind='decoy hash', hash=h['hash'], name=h['name'], character_model_cost_bits=sum(SR.name_costs(table, h['name']))/8) for h in d['hits']['decoy'] if h['kind'] == 'key']
        entry['control_agreements'] = chance
        entry['cheapest_control_agreement_bits'] = min((c['character_model_cost_bits'] for c in chance), default=None)
        lengths[str(n)] = entry
        for h in d['hits']['real']:
            if h['kind'] != 'key':
                continue
            value = int(h['hash'], 16); ok, total = K.agree(keys, value, h['name'])
            assert digest(h['name']) == value and (ok, total) == (h['agree'], h['records'])
            t = by_hash[h['hash']]; cost = sum(SR.name_costs(table, h['name']))/8
            row = dict(hash=h['hash'], name=h['name'], length=n, keys_agreeing=ok, keys_of_the_hash=total, group=t['group'], character_model_cost_bits=cost,
                       rows=' '.join('%s:%d' % (c, i) for c, i, _ in keys[value][:4]), label=t.get('label', ''))
            if t['group'] == 'known answer':
                found_known.setdefault(h['hash'], []).append(h['name'])
            else:
                row['status'] = 'key-confirmed candidate, not accepted'
            names.append(row)
    known = [t for t in targets if t['group'] == 'known answer' and str(len(t['name'])) in lengths]
    right = sum(1 for t in known if t['name'] in found_known.get(t['hash'], [])); extra = sum(len([x for x in found_known.get(t['hash'], []) if x != t['name']]) for t in known)
    summary = dict(lengths_done=sorted(int(k) for k in lengths), counts=dict(co.Counter(t['group'] for t in targets)),
                   known_answers=dict(names=len(known), returned=right, other_names_returned_for_these_hashes=extra),
                   by_length=lengths, names=[r for r in names if r['group'] != 'known answer'])
    (HERE/'keyed_short_results.json').write_text(json.dumps(summary, indent=1)+NL, encoding='utf-8')
    fields = ['hash', 'name', 'length', 'keys_agreeing', 'keys_of_the_hash', 'group', 'character_model_cost_bits', 'rows', 'label', 'status']
    with (HERE/'keyed_short_names.csv').open('w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fields, extrasaction='ignore'); writer.writeheader(); writer.writerows(r for r in names if r['group'] != 'known answer')
    print(json.dumps({k: v for k, v in summary.items() if k != 'names'}, indent=1))
    for r in names:
        if r['group'] != 'known answer':
            print(r['group'], r['hash'], r['name'], '%d of %d keys' % (r['keys_agreeing'], r['keys_of_the_hash']), 'cost %.1f bits' % r['character_model_cost_bits'], r['rows'], r['label'])


if __name__ == '__main__':
    if sys.argv[1] == 'targets':
        document = build_targets(); text = json.dumps(document, indent=1)+NL
        if TARGETS.exists():
            assert TARGETS.read_text(encoding='utf-8') == text, 'frozen targets differ'
        else:
            TARGETS.write_text(text, encoding='utf-8')
        print(document['counts'])
    elif sys.argv[1] == 'run':
        run(int(sys.argv[2]))
    elif sys.argv[1] == 'report':
        report()
