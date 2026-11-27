"""Wide prefix search for name lengths 10..12 (the range the frozen tail solver does not cover).

The Kamui hash of a 5..12 byte name is also invertible in its last four bytes once the bytes
before them are fixed (see name_judge_short_solver.c). Lengths 10..12 cost at most
|alphabet|^2 solves per (prefix, target). The ranked prefixes come from the same frozen
machinery as the long search (MoveModel head prior, SubwordGrammar, native enumerator); only
the 13..24 length check of GenericPrefixPlan is lifted, in a subclass.

Self-test: python name_judge_short.py --self-test   (every verified T8 move name of length
9..12 must be recovered from its own prefix and hash, and nothing else may verify wrongly)
"""
import t8names_paths as _P
import collections as co
import hashlib
import json
import math
from pathlib import Path
import subprocess

from name_judge_data import HERE
from typed_search import GenericPrefixPlan

CC = _P.CC
BIN = HERE/'name_judge_runs'/'bin'
SHORT_LENGTHS = (10, 11, 12)


def solver():
    source = _P.SRC/'name_judge_short_solver.c'
    key = hashlib.sha256(source.read_bytes()+(_P.SRC/'kamui_search.c').read_bytes()).hexdigest()[:16]
    exe = BIN/('name_judge_short_solver_%s.exe' % key)
    if not exe.exists():
        BIN.mkdir(parents=True, exist_ok=True)
        subprocess.run([CC, '-O3', '-Wall', '-o', str(exe), str(source)], check=True)
    return exe


class ShortPrefixPlan(GenericPrefixPlan):
    """GenericPrefixPlan for total lengths below 13. Same costs, same head pool; no other change."""
    def __init__(self, language, weighted_heads, length, namespace='move', model_id='typed_short'):
        if not 9 <= length <= 12:
            raise ValueError('short plan covers lengths 9..12')
        self.namespace = namespace; self.model_id = model_id; self.language = language; self.length = length
        weights = co.Counter()
        for weight, head in weighted_heads:
            if not head or not head.isascii() or any(ord(c) < 33 or ord(c) > 126 or c in '^$?' for c in head):
                raise ValueError('Heads must be nonempty printable native tokens')
            if not math.isfinite(weight) or weight <= 0:
                continue
            if language.mass(length-len(head), language.start) > 0:
                weights[head] += weight
        ordered = sorted(weights)
        if any(b.startswith(a) for a, b in zip(ordered, ordered[1:])):
            raise ValueError('Head pool must be prefix-free')
        total = sum(weights.values()); self.heads = {}
        for head, weight in sorted(weights.items()):
            rest = length-len(head); headcost = language.quantize(-math.log2(weight/total))
            self.heads[head] = dict(depth=max(0, rest-4), rest_length=rest,
                                    offset=headcost-language.normalizer(rest) if rest >= 4 else headcost, singleton=rest < 4)
        self.alphabet = ''.join(sorted(set(language.alphabet) | set(''.join(self.heads))))
        self.count_groups = co.Counter((v['depth'], v['offset'], v['singleton']) for v in self.heads.values())
        self.head_lengths = sorted({len(h) for h in self.heads})


def plan(model, language, codes, length):
    if length >= 13:
        return model.plan(language, codes, length)
    return ShortPrefixPlan(language, model.weighted_heads(codes, length), length, namespace='move', model_id=model.model_id+'_short')


def self_test():
    from name_judge_data import load_t8_rows
    from recover import digest
    rows = load_t8_rows(); names = sorted({r['move_name'] for r in rows if r['move_status'] == 'verified' and 9 <= len(r['move_name']) <= 12
                                           and r['move_name'].isascii()})
    alphabet = ''.join(sorted({c for n in names for c in n}))
    exe = solver(); directory = HERE/'name_judge_runs'/'short_selftest'; directory.mkdir(parents=True, exist_ok=True)
    report = {}
    for length in (9, 10, 11, 12):
        group = [n for n in names if len(n) == length][:(40 if length == 9 else 400)]
        # decoy hashes ride along: they must produce only names that really hash to them
        hashes = sorted({digest(n) for n in group} | {digest(n) ^ 0x13371337 for n in group[:20]})
        (directory/'targets.txt').write_text(''.join('%08X 0\n' % h for h in hashes), encoding='ascii')
        (directory/'prefixes.txt').write_text(''.join(sorted({n[:-4] for n in group}))if False else ''.join(p+'\n' for p in sorted({n[:-4] for n in group})), encoding='ascii')
        run = subprocess.run([str(exe), str(directory/'targets.txt'), str(directory/'prefixes.txt'), alphabet], capture_output=True, text=True, check=True)
        found = set()
        for line in run.stdout.splitlines():
            hh, name = line.split('\t'); assert digest(name) == int(hh, 16) and len(name) == length
            found.add(name)
        stats = json.loads(run.stderr); assert stats['hits'] == len(run.stdout.splitlines())
        missing = [n for n in group if n not in found]
        report[length] = dict(names=len(group), recovered=len(group)-len(missing), other_preimages=len(found-set(group)), solver=stats)
        assert not missing, missing[:5]
    (HERE/'name_judge_short_selftest.json').write_text(json.dumps(report, indent=1)+'\n')
    print(json.dumps(report, indent=1))


if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser(); parser.add_argument('--self-test', action='store_true'); args = parser.parse_args()
    if args.self_test:
        self_test()
