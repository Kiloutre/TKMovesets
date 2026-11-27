"""Shared read-only data layer for the name judge.

Everything here is local and read-only with respect to existing artifacts.
Caches are written next to this file with the prefix ``name_judge_``.

* typed corpus rows / labels (T8 verified, T7)
* compact move structure for every retail T8 move (from <workspace>/game/movesets)
* older-game move lists (Tag2 / T6 / T5 / T4 extracts, T7 from the typed corpus)
"""
import t8names_paths as _P
import collections as co
import gzip
import hashlib
import json
from pathlib import Path
import sys

HERE = _P.LAB
REPO = None  # locations come from t8names_paths
MOVESETS = _P.MOVESETS
LEGACY = _P.LEGACY
STRUCTURE_CACHE = HERE/'name_judge_structure.json.gz'
LEGACY_CACHE = HERE/'name_judge_legacy.json.gz'
MASKS = (0, 0x13371337, 0x5A5A5A5A, 0xA5A5A5A5, 0x0F0F0F0F)

GROUP_CANCEL = 0x8012
GROUP_END = 0x8013
CANCEL_END = 0x8000
SEQUENCE_BASE = 0x8014


def sha_file(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def bucket(text, salt, modulus):
    return int.from_bytes(hashlib.sha256((salt+':'+text).encode()).digest()[:8], 'big') % modulus


def load_rows():
    with gzip.open(HERE/'typed_corpus'/'rows.jsonl.gz', 'rt', encoding='utf-8') as f:
        return [json.loads(line) for line in f]


def load_t8_rows():
    """T8 rows only, with the few fields the judge needs (small memory footprint)."""
    out = []; intern = sys.intern
    with gzip.open(HERE/'typed_corpus'/'rows.jsonl.gz', 'rt', encoding='utf-8') as f:
        for line in f:
            if '"game":"T8"' not in line:
                continue
            r = json.loads(line)
            out.append(dict(game='T8', code=intern(r['code']), id=int(r['id']), move_hash=r['move_hash'], move_name=r['move_name'] or '',
                            move_status=intern(r['move_status']), anim_name=r['anim_name'] or '', anim_status=intern(r['anim_status'])))
    return out


def load_labels():
    return json.loads((HERE/'typed_corpus'/'labels.json').read_text(encoding='utf-8'))


def load_targets(namespace='move'):
    return json.loads((HERE/'typed_corpus'/('unresolved_%s_targets.json' % namespace)).read_text(encoding='utf-8'))


def load_overlay():
    import csv
    with (HERE.parent/'accepted_moves_overlay.csv').open(encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f))


def load_all_overlays():
    """Every owner-accepted move name on disk: {hash: name} from all accepted_moves_overlay*.csv files (read only)."""
    import csv
    out = {}
    for path in sorted(HERE.parent.glob('accepted_moves_overlay*.csv')):
        with path.open(encoding='utf-8', newline='') as f:
            for row in csv.DictReader(f):
                if row.get('namespace', 'move') == 'move' and row.get('hash') and row.get('name'):
                    out[row['hash']] = row['name']
    return out


# ----------------------------------------------------------------------------
# Move structure
# ----------------------------------------------------------------------------

def _resolve(move, aliases, count):
    """Cancel/transition target -> move id in this list, or -1."""
    if move is None:
        return -1
    if move >= 0x8000:
        index = move-0x8000
        if 0 <= index < len(aliases):
            move = aliases[index]
        else:
            return -1
    return move if 0 <= move < count else -1


def _structure_of(path):
    data = json.loads(Path(path).read_text(encoding='utf-8'))
    moves = data['moves']; tables = data['tables']; header = data['header']
    aliases = header.get('current_aliases') or []
    count = len(moves)
    groups = tables.get('group_cancels', [])
    sequences = tables.get('input_sequences', []); inputs = tables.get('inputs', [])

    def command(text):
        value = int(text, 16) if isinstance(text, str) else int(text)
        return value & 0xFFFFFFFF, value >> 32

    def sequence(lo):
        index = lo-SEQUENCE_BASE
        if not 0 <= index < len(sequences):
            return None
        seq = sequences[index]; start = seq.get('extradata', 0); n = seq.get('u2', 0)
        out = []
        for row in inputs[start:start+n]:
            if isinstance(row, list) and len(row) == 2:
                out.append((row[0] & 0xFFFFFFFF, row[1] & 0xFFFFFFFF))
            elif isinstance(row, dict):
                lo2, hi2 = command(row.get('command', 0)); out.append((lo2, hi2))
        return out

    incoming = co.defaultdict(list); out_edges = []
    reaction_targets = set()
    for row in tables.get('reactions', []):
        for key, value in row.items():
            if key in ('pushbacks',) or not isinstance(value, int):
                continue
            if key.endswith('direction') or key.endswith('rotation') or key == 'vertical_pushback':
                continue
            target = _resolve(value, aliases, count)
            if target >= 0:
                reaction_targets.add(target)
    transition_sources = co.defaultdict(list)
    records = []
    for index, move in enumerate(moves):
        edges = []
        for cancel in move.get('cancels', []):
            lo, hi = command(cancel['command'])
            if lo == GROUP_CANCEL and hi == 0:
                start = cancel['move']
                for g in groups[start:start+400]:
                    glo, ghi = command(g['command'])
                    if glo == GROUP_END and ghi == 0:
                        break
                    edges.append((glo, ghi, _resolve(g['move'], aliases, count), 1,
                                  g.get('window', [0, 0])[0], g.get('option', 0)))
            else:
                edges.append((lo, hi, _resolve(cancel['move'], aliases, count), 0,
                              cancel.get('window', [0, 0])[0], cancel.get('option', 0)))
        seen = set(); own = []
        for lo, hi, target, group, window, option in edges:
            seq = sequence(lo) if SEQUENCE_BASE <= lo < 0x10000 else None
            key = (lo, hi, target)
            if key in seen:
                continue
            seen.add(key)
            own.append([lo, hi, target, group, seq])
            if target >= 0 and target != index:
                incoming[target].append([lo, hi, index, group, seq])
        transition = _resolve(move.get('transition'), aliases, count)
        if transition >= 0 and transition != index:
            transition_sources[transition].append(index)
        damage = max([h.get('damage', 0) for h in move.get('hit_conditions', [])] or [0])
        hitboxes = [h for h in move.get('hitboxes', []) if h.get('location') not in (None, '0x0', 0) or h.get('startup')]
        throw = any(isinstance(p, dict) and p.get('id') in (0x8000+0x1c0,) for p in move.get('extra_props', []))
        records.append(dict(
            id=index, name=move.get('name') or '', h=move.get('name_hash'), anim=move.get('anim_name') or '',
            ah=move.get('anim_hash'), hl=int(move.get('hitlevel', '0x0'), 16) if isinstance(move.get('hitlevel'), str) else int(move.get('hitlevel') or 0),
            vu=int(move.get('vuln', '0x0'), 16) if isinstance(move.get('vuln'), str) else int(move.get('vuln') or 0),
            su=move.get('startup', 0), rc=move.get('recovery', 0), tr=transition,
            air=[move.get('airborne_start', 0), move.get('airborne_end', 0)], gf=move.get('ground_fall', 0),
            nhb=len(hitboxes), dmg=damage, voice=move.get('voiceclip') is not None,
            nx=len(move.get('extra_props', [])), ns=len(move.get('start_props', [])), ne=len(move.get('end_props', [])),
            props=sorted({p.get('id') for p in move.get('extra_props', []) if isinstance(p, dict) and p.get('id')})[:24],
            sprops=sorted({p.get('id') for p in move.get('start_props', []) if isinstance(p, dict) and p.get('id')})[:8],
            eprops=sorted({p.get('id') for p in move.get('end_props', []) if isinstance(p, dict) and p.get('id')})[:8],
            amax=move.get('anim_max_len', 0), u15=move.get('u15', 0),
            out=own[:60], nout=len(own)))
    for index, record in enumerate(records):
        inc = incoming.get(index, [])
        # keep the most informative incoming edges first: explicit input, then others
        inc.sort(key=lambda e: (not (e[1] & 0xFF or (e[0] and e[0] < 0x8000) or e[4]), e[2]))
        record['inc'] = inc[:40]; record['ninc'] = len(inc)
        record['rx'] = index in reaction_targets
        record['trsrc'] = transition_sources.get(index, [])[:12]
    return dict(code=data.get('code') or Path(path).stem, aliases=aliases, moves=records)


def build_structure():
    out = {}
    for path in sorted(MOVESETS.glob('*.json')):
        doc = _structure_of(path); out[doc['code']] = doc
        print('structure', doc['code'], len(doc['moves']), flush=True)
    raw = json.dumps(out, separators=(',', ':')).encode()
    STRUCTURE_CACHE.write_bytes(gzip.compress(raw, compresslevel=5, mtime=0))
    return out


_STRUCTURE = None


def load_structure():
    global _STRUCTURE
    if _STRUCTURE is None:
        if not STRUCTURE_CACHE.exists():
            build_structure()
        _STRUCTURE = json.loads(gzip.decompress(STRUCTURE_CACHE.read_bytes()))
    return _STRUCTURE


# ----------------------------------------------------------------------------
# Older games
# ----------------------------------------------------------------------------

def _legacy_structure(doc):
    """Per move of an extractor JSON: incoming commands, hit level, cancel count."""
    moves = doc.get('moves', []); cancels = doc.get('cancels', []); groups = doc.get('group_cancels', [])
    aliases = doc.get('aliases', []); count = len(moves)
    version = str(doc.get('version', ''))
    group_marker, group_end = (0x800B, 0x800C) if 'Tekken7' in version or version.startswith('Tekken7') else (0x800B, 0x800C)
    incoming = co.defaultdict(list); outs = []
    for index, move in enumerate(moves):
        start = move.get('cancel_idx', -1); own = []
        if isinstance(start, int) and 0 <= start < len(cancels):
            for cancel in cancels[start:start+300]:
                cmd = cancel.get('command', 0)
                if cmd == 0x8000:
                    own.append((cmd, cancel.get('move_id', -1), 0)); break
                if cmd == group_marker:
                    for g in groups[cancel.get('move_id', 0):cancel.get('move_id', 0)+400]:
                        if g.get('command') == group_end:
                            break
                        own.append((g.get('command', 0), g.get('move_id', -1), 1))
                else:
                    own.append((cmd, cancel.get('move_id', -1), 0))
        resolved = []
        for cmd, target, group in own:
            target = _resolve(target, aliases, count)
            resolved.append((cmd & 0xFFFFFFFF, cmd >> 32, target))
            if target >= 0 and target != index:
                incoming[target].append((cmd & 0xFFFFFFFF, cmd >> 32, index))
        outs.append(resolved)
    out = []
    for index, move in enumerate(moves):
        inc = incoming.get(index, [])
        out.append(dict(name=move.get('name') or '', anim=move.get('anim_name') or '', hl=move.get('hitlevel', 0),
                        inc=[list(e) for e in inc[:30]], ninc=len(inc), nout=len(outs[index]),
                        tr=_resolve(move.get('transition'), aliases, count)))
    return out


def build_legacy():
    out = {}
    for directory in sorted(p for p in (LEGACY.iterdir() if LEGACY.exists() else ()) if p.is_dir()):     # no older-game extracts: empty
        game = directory.name.split('_')[0]
        if game not in ('tag2', 'rpcs3', 't6', 't5', 't4', 't7'):
            continue
        path = directory/(directory.name+'.json')
        if not path.exists():
            continue
        doc = json.loads(path.read_bytes().decode('utf-8', 'replace'))
        out[directory.name] = dict(game='tag2' if game == 'rpcs3' else game, moves=_legacy_structure(doc))
        print('legacy', directory.name, len(out[directory.name]['moves']), flush=True)
    raw = json.dumps(out, separators=(',', ':')).encode()
    LEGACY_CACHE.write_bytes(gzip.compress(raw, compresslevel=5, mtime=0))
    return out


_LEGACY = None


def load_legacy():
    global _LEGACY
    if _LEGACY is None:
        if not LEGACY_CACHE.exists():
            build_legacy()
        _LEGACY = json.loads(gzip.decompress(LEGACY_CACHE.read_bytes()))
    return _LEGACY


SLIM_CACHE = HERE/'name_judge_structure_slim.json.gz'


def load_structure_slim():
    """Small form of the structure cache: {'atoms': [...], 'codes': {code: [[atom ids], incoming, outgoing, transition sources]}}.

    incoming = [[direction digits, button mask, source move id], ...]. Built once from the full cache.
    """
    if not SLIM_CACHE.exists():
        import name_judge as J
        vocabulary = {}; codes = {}
        for path in sorted(MOVESETS.glob('*.json')):
            doc = _structure_of(path); moves = []
            for move in doc['moves']:
                atoms, incoming, outgoing, sources = J.slim_move(move)
                moves.append([[vocabulary.setdefault(a, len(vocabulary)) for a in atoms], [list(e) for e in incoming], list(outgoing), list(sources)])
            codes[doc['code']] = moves
        ordered = [a for a, i in sorted(vocabulary.items(), key=lambda kv: kv[1])]
        SLIM_CACHE.write_bytes(gzip.compress(json.dumps(dict(atoms=ordered, codes=codes), separators=(',', ':')).encode(), compresslevel=5, mtime=0))
    return json.loads(gzip.decompress(SLIM_CACHE.read_bytes()))


LEGACY_NAMES_CACHE = HERE/'name_judge_legacy_names.json.gz'


def load_legacy_names():
    """{list key: [move name, ...]} in list order; names only (small)."""
    if not LEGACY_NAMES_CACHE.exists():
        names = {key: [m['name'] for m in doc['moves']] for key, doc in load_legacy().items()}
        LEGACY_NAMES_CACHE.write_bytes(gzip.compress(json.dumps(names, separators=(',', ':')).encode(), compresslevel=5, mtime=0))
    return json.loads(gzip.decompress(LEGACY_NAMES_CACHE.read_bytes()))


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(); ap.add_argument('--structure', action='store_true'); ap.add_argument('--legacy', action='store_true')
    a = ap.parse_args()
    if a.structure:
        build_structure()
    if a.legacy:
        build_legacy()
