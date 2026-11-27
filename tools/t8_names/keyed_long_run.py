"""Keyed search of move names of length 11 to 20 (keyed_long.c): the first characters within a character-model
budget, the last four from the hashes, every result decided by the move key.

One character model for all lengths: everything known (verified names, owner-accepted names, the public
dictionary's extra names) MINUS the known-answer names below, which are hidden from it.

Wanted hashes (no length and no prefix supplied; every hash is searched at every length):
  open            the move hashes with no accepted name
  doubtful label  the eleven verified names whose key no list code reproduces: searched as if they had no name
  known answer    hidden verified names whose key agrees:
                    lengths 11 and 12   the evaluation names of exhaustive_10_12_cases.json (already hidden there)
                    lengths 13 to 20    a frozen 6% sample
                  For lengths 11 and 12 the work grows with the number of wanted hashes, so only 100 known answers
                  of each of the two lengths are looked up there; for 13 to 20 all of them are.

Controls are built into the tool: hash ^ mask (four frozen masks), key ^ mask (lengths 13 to 20), and the code of
another list. Known-answer result: a hidden name must be returned exactly when its body is within the budget.

  python keyed_long_run.py sample                   freeze the wanted hashes and write the character model
  python keyed_long_run.py count <n> <bits>         number of bodies within a budget (no hashing), to choose budgets
  python keyed_long_run.py run <n> <bits>           one length -> keyed_runs/long/n<n>.json
  python keyed_long_run.py report                   -> keyed_long_results.json, keyed_long_names.csv
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

from name_judge_data import HERE, bucket, load_all_overlays, load_labels, load_t8_rows, load_targets
import short_exhaustive_run as SR
import exhaustive_10_12_run as R10
import keyed_short_run as KS
import keyed_context
import move_key as K
from recover import digest

CC = _P.CC
ROUND = int(os.environ.get('KEYED_ROUND', '1'))          # round 2 and later: the key-confirmed names of the rounds before are added to the character model
RUNS = HERE/'keyed_runs'/('long' if ROUND == 1 else 'long_round%d' % ROUND)
SUFFIX = '' if ROUND == 1 else '_round%d' % ROUND
SAMPLE = HERE/'keyed_long_targets.json'
LENGTHS = tuple(range(11, 21))
SALT = 'keyed-long-known-answers-v1'
PER_MILLE = 60
PAIR_MODE_KNOWN = 100                       # known answers of each of the lengths 11 and 12 looked up in the runs of lengths 11 and 12
WORKERS = int(os.environ.get('EXHAUSTIVE_WORKERS', '10'))
ALPHABET = SR.ALPHABET
NL = chr(10); TAB = chr(9)


def body_length(n):
    return n-4 if (n <= 12 or n >= 15) else n-3


def build_sample():
    keys = K.load_keys(); overlay = load_all_overlays()
    out = [dict(hash=t['hash'], group='open') for t in load_targets('move') if t['hash'] not in overlay]
    out += [dict(hash='0x%08X' % digest(name), group='doubtful label', label=name) for name in KS.DOUBTFUL]
    hidden_short = [c for c in R10.load_cases()['cases'] if c['split'] == 'eval']          # lengths 10 to 12, hidden from the character model
    for n in (11, 12):
        pool = sorted((bucket(c['name'], SALT, 1 << 30), c['name'], c['hash']) for c in hidden_short if c['length'] == n and c['name'] not in KS.DOUBTFUL)
        kept = 0
        for _, name, h in pool:
            ok, total = K.agree(keys, int(h, 16), name)
            if total and ok == total and kept < PAIR_MODE_KNOWN:
                out.append(dict(hash=h, group='known answer', name=name)); kept += 1
    labels = [x for x in load_labels() if x['game'] == 'T8' and x['namespace'] == 'move' and x['status'] == 'verified' and len(x['source_hashes']) == 1]
    names_of = co.defaultdict(set)
    for x in labels:
        names_of[x['source_hashes'][0]].add(x['name'])
    for x in sorted(labels, key=lambda x: x['name']):
        name = x['name']; h = int(x['source_hashes'][0], 16)
        if not (13 <= len(name) <= 20) or not SR.native_name(name) or len(names_of[x['source_hashes'][0]]) != 1 or name in KS.DOUBTFUL:
            continue
        if bucket(name, SALT, 1000) >= PER_MILLE:
            continue
        ok, total = K.agree(keys, h, name)
        if total and ok == total:
            out.append(dict(hash='0x%08X' % h, group='known answer', name=name))
    assert len({t['hash'] for t in out}) == len(out)
    hidden = sorted({c['name'] for c in hidden_short} | {t['name'] for t in out if t['group'] == 'known answer'})
    by_length = co.Counter(len(t['name']) for t in out if t['group'] == 'known answer')
    return dict(schema_version=1, salt=SALT, per_mille=PER_MILLE, counts=dict(co.Counter(t['group'] for t in out)),
                known_answers_by_length={str(k): v for k, v in sorted(by_length.items())}, names_hidden_from_the_character_model=len(hidden), hidden=hidden, targets=out)


def load():
    return json.loads(SAMPLE.read_text(encoding='utf-8'))


def vocabulary(hidden):
    verified = {r['move_name'] for r in load_t8_rows() if r['move_status'] == 'verified' and r['move_name'] and SR.native_name(r['move_name'])}
    accepted = {n for n in load_all_overlays().values() if SR.native_name(n)}
    names = (verified | accepted | SR.dictionary_extra())-set(hidden)
    if ROUND > 1:
        # key-confirmed candidates (not accepted names): they only change which bodies are enumerated; the key still decides every result
        with (HERE/'keyed_confirmed_all.csv').open(encoding='utf-8', newline='') as f:
            names |= {r['name'] for r in csv.DictReader(f) if r['verdict'] == 'real' and SR.native_name(r['name'])}
    return names


def model():
    path = RUNS/'lm.bin'
    if not path.exists():
        RUNS.mkdir(parents=True, exist_ok=True); SR.build_lm(vocabulary(load()['hidden'])).tofile(path)
    return path


def binary():
    source = _P.SRC/'keyed_long.c'
    key = hashlib.sha256(source.read_bytes()+(_P.SRC/'kamui_search.c').read_bytes()).hexdigest()[:16]
    exe = HERE/'keyed_runs'/'bin'/('keyed_long_%s.exe' % key)
    if not exe.exists():
        exe.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([CC, '-O3', '-march=native', '-o', str(exe), str(source)], check=True)
    return exe


def jobs_for(table):
    share = 2.0**(-table[0, 0, 0, 1:].astype(np.float64)/SR.QUANTUM); jobs = []
    for first in range(len(ALPHABET)):
        jobs += [(first, second) for second in range(len(ALPHABET))] if share[first] >= 0.02 else [(first, None)]
    return jobs


def wanted(n, sample):
    """Targets looked up at length n."""
    out = []
    for t in sample['targets']:
        if t['group'] == 'known answer' and n <= 12 and len(t['name']) > 12:
            continue                      # lengths 11 and 12: the work grows with the wanted hashes
        out.append(t)
    return out


def run(n, bits, count_only=False):
    from concurrent.futures import ThreadPoolExecutor
    RUNS.mkdir(parents=True, exist_ok=True); out_path = RUNS/('n%d.json' % n)
    if out_path.exists() and not count_only:
        earlier = json.loads(out_path.read_text(encoding='utf-8'))['budget_bits']
        if earlier >= bits:
            print('exists', out_path, 'with budget', earlier); return
        # a larger budget covers everything the smaller one did: the earlier result is kept under its own name
        out_path.replace(RUNS/('n%d_budget_%g.json' % (n, earlier)))
    sample = load(); keys = K.load_keys(); lm_path = model(); table = np.fromfile(lm_path, dtype=np.uint16).reshape(64, 64, 64, 64)
    exe = binary(); started = time.perf_counter(); budget = int(round(bits*8)); targets = wanted(n, sample)
    tf = RUNS/('targets_n%d%s.txt' % (n, '_count' if count_only else ''))
    if count_only:
        tf.write_text('', encoding='ascii')
    else:
        KS.write_targets(tf, [(int(t['hash'], 16), keys[int(t['hash'], 16)]) for t in targets], keys)

    def job(item):
        first, second = item; last = None
        for attempt in range(4):
            try:
                done = subprocess.run([str(exe), str(lm_path), ALPHABET, str(tf), str(n), str(budget), str(first)]+([] if second is None else [str(second)]),
                                      stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
                                      creationflags=getattr(subprocess, 'BELOW_NORMAL_PRIORITY_CLASS', 0))      # the workers yield to anything the owner runs
                return json.loads(done.stderr.decode()), done.stdout.decode('ascii').splitlines()
            except (OSError, MemoryError, subprocess.CalledProcessError, ValueError) as error:
                last = error; print('retry', n, item, attempt+1, repr(error)[:200], flush=True); time.sleep(15*(attempt+1))
        raise RuntimeError('keyed search job failed: %r' % (last,))

    totals = co.Counter(); hits = []
    with ThreadPoolExecutor(WORKERS) as pool:
        for stats, lines in pool.map(job, jobs_for(table)):
            for field in ('bodies', 'delta_matches', 'names_derived', 'key', 'hmask', 'kmask', 'wrongcode'):
                totals[field] += stats[field]
            totals['processor_seconds'] += stats['seconds']; pairs = stats['pairs']; codes = stats['codes']
            for line in lines:
                kind, value, name, agree, total, body_cost, tail_cost = line.split(TAB)
                assert len(name) == n
                hits.append(dict(kind=kind, hash='0x'+value, name=name, agree=int(agree), records=int(total), body_cost_bits=int(body_cost)/8, tail_cost_bits=int(tail_cost)/8))
    tf.unlink()
    if count_only:
        print('length', n, 'body', body_length(n), 'budget %.1f bits' % bits, 'bodies %.4g' % totals['bodies'], 'processor seconds %.0f' % totals['processor_seconds'], 'wall %.0f s' % (time.perf_counter()-started))
        return
    document = dict(length=n, body_length=body_length(n), budget_bits=bits, wanted_hashes=len(targets), pairs_looked_up=pairs, list_codes=codes, totals=dict(totals), hits=hits,
                    wall_seconds=time.perf_counter()-started)
    out_path.write_text(json.dumps(document, indent=1)+NL, encoding='utf-8')
    print('length', n, 'body', body_length(n), 'budget', bits, dict(totals), 'wall %.0f s' % document['wall_seconds'], flush=True)
    for h in hits:
        print('   ', h)


def taken_as_real(reading):
    return reading.startswith('every one of') or 'cheaper than chance' in reading or 'shares a part with the animation' in reading or 'starts like a named neighbour' in reading


def report():
    sample = load(); keys = K.load_keys(); by_hash = {t['hash']: t for t in sample['targets']}
    table = np.fromfile(model(), dtype=np.uint16).reshape(64, 64, 64, 64)

    def body_cost(name):
        """Cost of the first body_length characters under the model that hides the known answers (bits)."""
        codes = [0, 0, 0]+[ALPHABET.index(ch)+1 for ch in name[:body_length(len(name))]]
        return sum(int(table[codes[i], codes[i+1], codes[i+2], codes[i+3]]) for i in range(len(codes)-3))/8

    lengths = {}; names = []; returned = co.defaultdict(list); pools = co.defaultdict(list)
    for n in LENGTHS:
        path = RUNS/('n%d.json' % n)
        if not path.exists():
            continue
        d = json.loads(path.read_text(encoding='utf-8')); budget = d['budget_bits']
        known = [t for t in wanted(n, sample) if t['group'] == 'known answer' and len(t['name']) == n]
        inside = [t for t in known if body_cost(t['name']) <= budget]
        for h in d['hits']:
            if h['kind'] != 'key':
                continue
            value = int(h['hash'], 16); ok, total = K.agree(keys, value, h['name'])
            assert digest(h['name']) == value and (ok, total) == (h['agree'], h['records']) and ok >= 1
            t = by_hash[h['hash']]
            if t['group'] == 'known answer':
                returned[h['hash']].append(h['name'])
            # the agreements for known-answer hashes get the same reading as the others: it shows what the reading rule does to true and to chance names
            names.append(dict(hash=h['hash'], name=h['name'], length=n, keys_agreeing=ok, keys_of_the_hash=total, group=t['group'], body_cost_bits=h['body_cost_bits'],
                              tail_cost_bits=h['tail_cost_bits'], rows=' '.join('%s:%d' % (c, i) for c, i, _ in keys[value][:4]), label=t.get('label', ''),
                              hidden_name=t.get('name', ''), status='key-confirmed candidate, not accepted'))
        # A control agreement is a chance name. Every control kind repeats the real experiment over all wanted hashes, so the number of chance
        # names expected among the open and doubtful hashes with a tail at least as cheap is (control agreements that cheap) / (control kinds) * share.
        controls = sorted(h['tail_cost_bits'] for h in d['hits'] if h['kind'] != 'key'); kinds_run = 9 if n >= 13 else 5
        share = sum(1 for t in wanted(n, sample) if t['group'] != 'known answer')/len(wanted(n, sample))
        pools['3 characters' if n in (13, 14) else '4 characters'] += controls
        for row in names:
            if row['length'] == n:
                row['chance_agreements_expected_at_this_length'] = round(len(controls)/kinds_run*(1-share if row['group'] == 'known answer' else share), 3)
        found = sum(1 for t in inside if t['name'] in returned.get(t['hash'], [])); beyond = sum(1 for t in known if t not in inside and t['name'] in returned.get(t['hash'], []))
        other = sum(len([x for x in returned.get(t['hash'], []) if x != t['name'] and len(x) == n]) for t in sample['targets'] if t['group'] == 'known answer')
        kinds = co.Counter(h['kind'][:5] for h in d['hits'])
        lengths[str(n)] = dict(body_length=d['body_length'], budget_bits=budget, bodies=d['totals']['bodies'], wanted_hashes=d['wanted_hashes'], pairs_looked_up=d['pairs_looked_up'],
                               known_answers=len(known), known_answers_within_the_budget=len(inside), share_within_the_budget=round(len(inside)/max(1, len(known)), 4),
                               known_answers_returned=found, returned_beyond_the_budget=beyond, other_names_returned_for_known_answer_hashes=other,
                               names_confirmed_by_the_key=d['totals']['key'], hash_mask_controls=d['totals']['hmask'], key_mask_controls=d['totals']['kmask'], wrong_code_controls=d['totals']['wrongcode'],
                               control_kinds=kinds_run, control_tail_costs_bits=controls, cheapest_control_tail_bits=controls[0] if controls else None,
                               processor_seconds=d['totals']['processor_seconds'], wall_seconds=d['wall_seconds'])
    # A one-key agreement can be a chance name. Chance names have random last characters: the cost of the tail of every control agreement of all
    # runs is pooled (tails of 3 new characters for lengths 13 and 14, of 4 otherwise), and the number of chance names expected among the open and
    # doubtful hashes of one length with a tail at least as cheap is (chance agreements expected at that length) * (share of pooled tails that cheap).
    # The share of random tails that cheap is measured directly as well: 50,000 random tails after the candidate's own body.
    rng = np.random.RandomState(20261117); DRAWS = 50000; context = keyed_context.Context(keys)
    for row in names:
        pool = pools['3 characters' if row['length'] in (13, 14) else '4 characters']
        cheaper = sum(c <= row['tail_cost_bits'] for c in pool)
        p = body_length(row['length']); fresh = row['length']-p; codes = [ALPHABET.index(ch)+1 for ch in row['name'][:p]]
        a = np.full(DRAWS, codes[-3]); b = np.full(DRAWS, codes[-2]); c = np.full(DRAWS, codes[-1]); cost = np.zeros(DRAWS, dtype=np.int64)
        for _ in range(fresh):
            x = rng.randint(1, len(ALPHABET)+1, DRAWS); cost += table[a, b, c, x]; a, b, c = b, c, x
        cost += table[a, b, c, 0]
        random_share = (int((cost <= row['tail_cost_bits']*8).sum())+1)/(DRAWS+1)
        expected = row['chance_agreements_expected_at_this_length']*random_share
        row['control_tails_this_cheap'] = '%d of %d' % (cheaper, len(pool)); row['share_of_random_tails_this_cheap'] = round(random_share, 6)
        row['chance_names_expected_with_a_tail_this_cheap'] = round(expected, 5)
        parts, like = context.of(row['name'], int(row['hash'], 16))
        row['context'] = '; '.join(x for x in ('starts like a named neighbour' if like else '', ('shares %s with the animation name' % ', '.join(parts)) if parts else '') if x)
        if row['keys_agreeing'] != row['keys_of_the_hash']:
            row['reading'] = 'only some keys agree: a chance name'
        elif row['keys_of_the_hash'] >= 2:
            row['reading'] = 'every one of %d keys agrees' % row['keys_of_the_hash']
        elif expected < 0.005:
            row['reading'] = 'one key; tail cheaper than chance tails'
        elif parts:
            row['reading'] = 'one key; shares a part with the animation name of its row'
        elif expected < 0.2 and like:
            row['reading'] = 'one key; tail fairly cheap and the name starts like a named neighbour'
        elif expected < 0.2:
            row['reading'] = 'one key; uncertain'
        else:
            row['reading'] = 'one key; not distinguishable from a chance name'
    # the reading rule on the known-answer hashes: true names against the chance agreements for the same hashes
    rule = co.Counter()
    for row in names:
        if row['group'] == 'known answer':
            what = 'hidden names' if row['name'] == row['hidden_name'] else 'chance agreements'
            rule[what] += 1
            taken = taken_as_real(row['reading'])
            rule[what+' taken as real'] += taken; rule[what+' left uncertain'] += row['reading'].endswith('uncertain')
    names = [row for row in names if row['group'] != 'known answer']
    # two names of different hashes that differ in one character confirm each other
    for row in names:
        partners = [o['name'] for o in names if o['hash'] != row['hash'] and len(o['name']) == len(row['name']) and sum(a != b for a, b in zip(o['name'], row['name'])) == 1]
        row['one_character_from'] = ' '.join(partners)
        if partners and row['reading'].startswith('one key'):
            row['reading'] += '; pair with '+partners[0]
    names.sort(key=lambda r: (r['length'], r['name']))
    summary = dict(lengths_done=sorted(int(k) for k in lengths), counts=sample['counts'], known_answers_by_length=sample['known_answers_by_length'], by_length=lengths,
                   reading_rule_on_known_answer_hashes=dict(rule), names=names)
    (HERE/('keyed_long_results%s.json' % SUFFIX)).write_text(json.dumps(summary, indent=1)+NL, encoding='utf-8')
    summary['pooled_control_tails'] = {k: dict(count=len(v), cheapest_bits=min(v) if v else None, quartiles_bits=[float(np.quantile(v, q)) for q in (0.05, 0.25, 0.5)] if v else None) for k, v in pools.items()}
    (HERE/('keyed_long_results%s.json' % SUFFIX)).write_text(json.dumps(summary, indent=1)+NL, encoding='utf-8')
    fields = ['hash', 'name', 'length', 'keys_agreeing', 'keys_of_the_hash', 'reading', 'context', 'chance_names_expected_with_a_tail_this_cheap', 'share_of_random_tails_this_cheap', 'control_tails_this_cheap',
              'chance_agreements_expected_at_this_length', 'one_character_from', 'group', 'body_cost_bits', 'tail_cost_bits', 'rows', 'label', 'status']
    with (HERE/('keyed_long_names%s.csv' % SUFFIX)).open('w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fields, extrasaction='ignore'); writer.writeheader(); writer.writerows(names)
    for n, e in lengths.items():
        print(n, {k: v for k, v in e.items() if k != 'control_tail_costs_bits'}, 'control tails', [round(c) for c in e['control_tail_costs_bits'][:12]])
    for r in names:
        print(r['length'], r['group'][:8], r['hash'], r['name'].ljust(20), '%d/%d' % (r['keys_agreeing'], r['keys_of_the_hash']), 'body %.1f tail %.1f' % (r['body_cost_bits'], r['tail_cost_bits']),
              'chance %.5f (random tails %.5f, controls %s)' % (r['chance_names_expected_with_a_tail_this_cheap'], r['share_of_random_tails_this_cheap'], r['control_tails_this_cheap']),
              '|', r['reading'], '|', r['rows'][:36], r['label'])
    print('pooled control tails', summary['pooled_control_tails'])
    print('reading rule on the known-answer hashes', dict(rule))


if __name__ == '__main__':
    command = sys.argv[1]
    if command == 'sample':
        document = build_sample(); text = json.dumps(document, indent=1)+NL
        if SAMPLE.exists():
            assert SAMPLE.read_text(encoding='utf-8') == text, 'frozen sample differs'
        else:
            SAMPLE.write_text(text, encoding='utf-8')
        print(document['counts'], document['known_answers_by_length'], 'hidden from the model:', document['names_hidden_from_the_character_model']); model()
    elif command == 'count':
        run(int(sys.argv[2]), float(sys.argv[3]), count_only=True)
    elif command == 'run':
        run(int(sys.argv[2]), float(sys.argv[3]))
    elif command == 'report':
        report()
