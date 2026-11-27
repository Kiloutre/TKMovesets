"""The second hash of a move name (found by the coordinating session, 2026-11-17, commit 9e52163).

Every retail move record holds, next to the hash of the move name, a key:

    move_key = Kamui(CODE + '_' + name)        CODE = the three-letter code of the move list in upper case

So a candidate name of a hash is checked against 32 more bits for every list the move is on. The values are
read from <workspace>/game/movenames/<code>.csv (columns id, name_hash, move_key), written by
motbin_retail.py from the retail motbins. Read only.

    KEYS = load_keys()             {name hash: [(code, id, key), ...]}   one entry per move record
    agree(KEYS, hash, name)        (records whose key the name reproduces, records of the hash)
    wrong_code(KEYS, hash, name)   the same with the code of another list: the control, must be (0, n)
"""
import t8names_paths as _P
import collections as co
import csv

from name_judge_data import HERE
from recover import digest

ROOT = None  # locations come from t8names_paths
MOVES = _P.MOVENAMES


def load_keys():
    table = co.defaultdict(list)
    for path in sorted(MOVES.glob('*.csv')):
        if len(path.stem) != 3:
            continue
        with path.open(encoding='utf-8', newline='') as handle:
            for r in csv.DictReader(handle):
                if r['name_hash'] and r['move_key']:
                    table[int(r['name_hash'], 16)].append((path.stem, int(r['id']), int(r['move_key'], 16)))
    return dict(table)


def codes_of(keys):
    return sorted({code for places in keys.values() for code, _, _ in places})


def agree(keys, value, name):
    places = keys.get(value, ())
    done = {}; ok = 0
    for code, _, key in places:
        if code not in done:
            done[code] = digest(code.upper()+'_'+name)
        ok += done[code] == key
    return ok, len(places)


_OTHER = {}


def wrong_code(keys, value, name):
    """Control: every record is tested with the code of the next list in alphabetical order."""
    if not _OTHER:
        codes = codes_of(keys)
        for i, code in enumerate(codes):
            _OTHER[code] = codes[(i+1) % len(codes)]
    places = keys.get(value, ())
    done = {}; ok = 0
    for code, _, key in places:
        if code not in done:
            done[code] = digest(_OTHER[code].upper()+'_'+name)
        ok += done[code] == key
    return ok, len(places)
