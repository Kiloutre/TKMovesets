"""Expansion of the key-confirmed names: small edits of every confirmed name, decided by the key.

Names of neighbouring moves differ by little (`sWALK_GUN00Left` / `sWALK_GUN00LeftE` / `sWALK_GUN00Right`). For every
name taken as real in keyed_confirmed_all.csv, these edits are hashed and looked up among the open hashes and the
doubtful labels that have no confirmed name yet:

  the last 1, 2 or 3 characters replaced by any string of that length
  1, 2 or 3 characters appended; the last 1, 2 or 3 characters removed
  one character replaced, inserted or removed at any position

A match on the name hash is then checked against the move key. The key decides: a wrong name passes one key with
probability 2^-32, and about three name-hash matches are expected by chance in the whole run.
New names are expanded in turn until nothing new appears.

  python keyed_expand.py     -> keyed_expand.json (and the names are picked up by keyed_report_tables.py)
"""
import csv
import itertools
import json
import time

from name_judge_data import HERE, load_all_overlays, load_targets
import keyed_short_run as KS
import move_key as K
from recover import digest

NL = chr(10)
ALPHABET = KS.ALPHABET


def edits(name):
    n = len(name)
    for k in (1, 2, 3):
        stem = name[:n-k]
        for tail in itertools.product(ALPHABET, repeat=k):
            t = ''.join(tail)
            yield stem+t
            yield name+t
        yield stem
    for i in range(n):
        yield name[:i]+name[i+1:]
        for ch in ALPHABET:
            yield name[:i]+ch+name[i+1:]
    for i in range(n+1):
        for ch in ALPHABET:
            yield name[:i]+ch+name[i:]


def main():
    started = time.perf_counter(); keys = K.load_keys(); overlay = {int(h, 16) for h in load_all_overlays()}
    wanted = ({int(t['hash'], 16) for t in load_targets('move')}-overlay) | {digest(x) for x in KS.DOUBTFUL}
    with (HERE/'keyed_confirmed_all.csv').open(encoding='utf-8', newline='') as f:
        seeds = [(r['name'], int(r['hash'], 16)) for r in csv.DictReader(f) if r['verdict'] == 'real']
    solved = {h for _, h in seeds}; queue = [name for name, _ in seeds]; done = set(); found = []; tried = 0; hash_matches = 0; rounds = 0
    while queue:
        rounds += 1; fresh = []
        for seed in queue:
            if seed in done:
                continue
            done.add(seed); seen = set()
            for candidate in edits(seed):
                if candidate in seen or not (1 <= len(candidate) <= 24):
                    continue
                seen.add(candidate); tried += 1; value = digest(candidate)
                if value in wanted and value not in solved:
                    hash_matches += 1; ok, total = K.agree(keys, value, candidate)
                    if ok:
                        found.append(dict(hash='0x%08X' % value, name=candidate, length=len(candidate), keys_agreeing=ok, keys_of_the_hash=total, from_name=seed,
                                          rows=' '.join('%s:%d' % (c, i) for c, i, _ in keys[value][:3])))
                        if ok == total:
                            solved.add(value); fresh.append(candidate)
                        print('found', found[-1], flush=True)
        queue = fresh
    # a rerun (after more names were confirmed elsewhere) adds to the earlier result
    path = HERE/'keyed_expand.json'; earlier = json.loads(path.read_text(encoding='utf-8')) if path.exists() else None
    if earlier:
        known = {(r['hash'], r['name']) for r in found}
        found = [r for r in earlier['names_confirmed_by_the_key'] if (r['hash'], r['name']) not in known]+found
        tried += earlier['edits_hashed']; hash_matches += earlier['name_hash_matches']
    report = dict(seed_names=len(seeds), names_expanded=len(done), rounds=rounds, runs=(earlier.get('runs', 1)+1) if earlier else 1, edits_hashed=tried, name_hash_matches=hash_matches,
                  chance_name_hash_matches_expected=round(tried*len(wanted)/2**32, 2), names_confirmed_by_the_key=found, seconds=time.perf_counter()-started)
    (HERE/'keyed_expand.json').write_text(json.dumps(report, indent=1)+NL, encoding='utf-8')
    print({k: v for k, v in report.items() if k != 'names_confirmed_by_the_key'})


if __name__ == '__main__':
    main()
