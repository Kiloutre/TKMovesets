"""Keyed search of T8 move names of length 21 to 24 (keyed_xl.c): body within a character-model budget, last four
characters from the name hash, the move key decides.

For these lengths the name is in the 13-24 branch of the hash, so its last four characters follow from the hash and the
first n - 4. CODE_name has 25 to 28 bytes (third branch), which gives no condition on the body alone: the tail is
derived for every wanted hash and the key is compared in full. Names above 24 characters are out of reach of this.

Wanted hashes: every open move hash, the eleven doubtful labels, and hidden known answers of length 21 to 24 (verified
single-hash names whose keys all agree, a salted 250 per mille; hidden from the character model). The search gets
hash and key only; no length and no prefix of any known answer is supplied.

The character model holds the verified and accepted names without the hidden ones, the public dictionary names, and
the key-confirmed candidates of the earlier rounds (keyed_confirmed_all.csv, verdict "real"); the key decides every result.

Everything else (splitting by first characters, the controls, the reading of one-key agreements, the report) is
keyed_long_run.py with its settings pointed at this search.

  python keyed_xl_run.py sample                   -> keyed_xl_targets.json (frozen)
  python keyed_xl_run.py count <n> <bits>         number of bodies within a budget (no hashing)
  python keyed_xl_run.py run <n> <bits>           one length -> keyed_runs/xl/n<n>.json
  python keyed_xl_run.py report                   -> keyed_long_results_xl.json, keyed_long_names_xl.csv
"""
import t8names_paths as _P
import collections as co
import hashlib
import json
import subprocess
import sys

from name_judge_data import HERE, bucket, load_all_overlays, load_labels, load_targets
import keyed_long_run as KL
import keyed_short_run as KS
import move_key as K
import short_exhaustive_run as SR
from recover import digest

LENGTHS = (21, 22, 23, 24)
PER_MILLE = 250
NL = chr(10)

# keyed_long_run.py reads these module settings at call time
KL.RUNS = HERE/'keyed_runs'/'xl'
KL.SUFFIX = '_xl'
KL.SAMPLE = HERE/'keyed_xl_targets.json'
KL.LENGTHS = LENGTHS
KL.ROUND = 2                                   # the key-confirmed candidates are in the character model


def binary():
    source = _P.SRC/'keyed_xl.c'
    key = hashlib.sha256(source.read_bytes()+(_P.SRC/'kamui_search.c').read_bytes()).hexdigest()[:16]
    exe = HERE/'keyed_runs'/'bin'/('keyed_xl_%s.exe' % key)
    if not exe.exists():
        exe.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([KL.CC, '-O3', '-march=native', '-o', str(exe), str(source)], check=True)
    return exe


KL.binary = binary


def build_sample():
    keys = K.load_keys(); overlay = load_all_overlays()
    out = [dict(hash=t['hash'], group='open') for t in load_targets('move') if t['hash'] not in overlay]
    out += [dict(hash='0x%08X' % digest(name), group='doubtful label', label=name) for name in KS.DOUBTFUL]
    labels = [x for x in load_labels() if x['game'] == 'T8' and x['namespace'] == 'move' and x['status'] == 'verified' and len(x['source_hashes']) == 1]
    names_of = co.defaultdict(set)
    for x in labels:
        names_of[x['source_hashes'][0]].add(x['name'])
    for x in sorted(labels, key=lambda x: x['name']):
        name = x['name']; h = int(x['source_hashes'][0], 16)
        if len(name) not in LENGTHS or not SR.native_name(name) or len(names_of[x['source_hashes'][0]]) != 1 or name in KS.DOUBTFUL:
            continue
        if bucket(name, KL.SALT, 1000) >= PER_MILLE:
            continue
        ok, total = K.agree(keys, h, name)
        if total and ok == total:
            out.append(dict(hash='0x%08X' % h, group='known answer', name=name))
    assert len({t['hash'] for t in out}) == len(out)
    hidden = sorted(t['name'] for t in out if t['group'] == 'known answer')
    by_length = co.Counter(len(t['name']) for t in out if t['group'] == 'known answer')
    return dict(schema_version=1, salt=KL.SALT, per_mille=PER_MILLE, counts=dict(co.Counter(t['group'] for t in out)),
                known_answers_by_length={str(k): v for k, v in sorted(by_length.items())}, names_hidden_from_the_character_model=len(hidden), hidden=hidden, targets=out)


def main():
    command = sys.argv[1]
    if command == 'sample':
        if KL.SAMPLE.exists():
            raise SystemExit('refusing to overwrite the frozen sample '+str(KL.SAMPLE))
        sample = build_sample(); KL.SAMPLE.write_text(json.dumps(sample, indent=1)+NL, encoding='utf-8')
        print({k: v for k, v in sample.items() if k not in ('targets', 'hidden')})
    elif command == 'count':
        KL.run(int(sys.argv[2]), float(sys.argv[3]), count_only=True)
    elif command == 'run':
        KL.run(int(sys.argv[2]), float(sys.argv[3]))
    elif command == 'report':
        KL.report()
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
