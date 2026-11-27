"""Merge every T8 retail name source into one table for TKMovesets2, and record every conflict and collision.

Sources (nothing here is edited):
  corpus      <workspace>/game/movenames/ALL_movenames_retail.csv (name and anim_name columns)
  public      name_keys.json of github.com/AliK3112/tekken-8-movesets-scripts (decimal hash -> name; namespace unknown)
  local       <workspace>/recovery/accepted.csv (animations) and every accepted_*_overlay*.csv
  correction  label_corrections_moves_*.csv ("correction recorded" = settled; anything else = candidate)
  candidate   names the acceptance rule left as candidates (left_as_candidate of the latest move-key batch)

Moves and animations stay separate namespaces. For a move hash the move key decides
(move_key = Kamui(CODE.upper() + "_" + name), one key per row): the name agreeing with the most keys is picked, ties by
source order settled correction > corpus > local > public. Animations have no key: corpus > local > public.
Unsettled candidates are never picked. A name that does not re-hash to its hash is never picked, except the corpus
placeholders of unnamed animations (ak<HASH>___, the underscores give the length): a placeholder stays when nothing
better exists, and is not counted as a candidate name in the conflict file.

Outputs (in --out):
  ALL_movenames_retail.csv   the corpus table with name / anim_name filled or corrected (same columns)
  extra_names.json           re-hashing public names of hashes that are in no row ({"0x%08X": name})
  name_conflicts.csv         one line per (namespace, hash, candidate name) for every hash with more than one
                             candidate name, or whose only label fails a check; says which name was picked and why
  merge_report.json          counts and input checksums

With --judge (the output of method_lab/judge_collisions.py: the name judge run on every collided move hash, all
collided names hidden from it) the judge's rank and score are written next to every collided move name. The move key
still decides; the judge adds two things: an unsettled candidate is picked when every move key agrees with it AND the
judge ranks it first, and a pick the judge ranks second is marked. The judge does not handle animations.

usage: merge_t8_names.py --public <name_keys.json> --out <folder> [--judge <judge_collisions.csv>]
"""
import t8names_paths as _P
import argparse
import collections as co
import csv
import hashlib
import json
import re
import sys
from pathlib import Path

ORDER = ('correction', 'corpus', 'local', 'public', 'candidate')
PLACEHOLDER = re.compile(r'ak[0-9A-F]{8}_*$')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--public', type=Path, required=True)
    ap.add_argument('--out', type=Path, default=_P.WORKSPACE/'merged')
    ap.add_argument('--judge', type=Path)
    a = ap.parse_args()
    rec = _P.RECOVERY
    from recover import digest
    assert digest('motOrigin') == 0x6AF2B483
    base = _P.MOVENAMES/'ALL_movenames_retail.csv'
    with base.open(encoding='utf-8-sig', newline='') as f:
        reader = csv.DictReader(f); fields = reader.fieldnames; rows = list(reader)
    inputs = {base.name: hashlib.sha256(base.read_bytes()).hexdigest(),
              'public name_keys.json': hashlib.sha256(a.public.read_bytes()).hexdigest()}

    judged = {}
    if a.judge:
        inputs['judge ' + a.judge.name] = hashlib.sha256(a.judge.read_bytes()).hexdigest()
        with a.judge.open(encoding='utf-8', newline='') as f:
            judged = {(int(r['hash'], 16), r['candidate_name']): (int(r['judge_rank']), r['judge_score']) for r in csv.DictReader(f)}
    # cand[(namespace, hash)][name] = set of sources
    cand = co.defaultdict(lambda: co.defaultdict(set))
    places = dict(move=co.defaultdict(list), animation=co.defaultdict(list))
    for r in rows:
        mh, ah = int(r['name_hash'], 16), int(r['anim_hash'], 16)
        places['move'][mh].append(r); places['animation'][ah].append(r)
        if r['name']: cand[('move', mh)][r['name']].add('corpus')
        if r['anim_name']: cand[('animation', ah)][r['anim_name']].add('corpus')
    public = {int(k): v for k, v in json.loads(a.public.read_text(encoding='utf-8')).items()}
    for h, name in public.items():
        for ns in ('move', 'animation'):
            if h in places[ns]: cand[(ns, h)][name].add('public')

    def read(path):
        inputs[path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
        with path.open(encoding='utf-8-sig', newline='') as f:
            return list(csv.DictReader(f))
    for path in sorted(rec.glob('accepted*.csv')):
        ns = 'move' if '_moves_' in path.name else 'animation'
        for r in read(path):
            assert r['status'] == 'accepted' and r.get('namespace', ns) == ns, (path.name, r)
            cand[(ns, int(r['hash'], 16))][r['name']].add('local:' + path.name)
    for path in sorted(rec.glob('label_corrections_moves_*.csv')):
        for r in read(path):
            kind = 'correction' if r['status'].startswith('correction recorded') else 'candidate'
            cand[('move', int(r['hash'], 16))][r['corrected_name']].add('%s:%s' % (kind, path.name))
    batches = sorted(rec.glob('accepted_moves_overlay_move_key_batch*.json'), key=lambda p: int(re.search(r'batch(\d+)', p.name).group(1)))
    for line in (json.loads(batches[-1].read_text(encoding='utf-8'))['left_as_candidate'] if batches else []):
        m = re.match(r'(\S+) \(0x([0-9A-F]{8})\)', line); assert m, line
        cand[('move', int(m.group(2), 16))][m.group(1)].add('candidate:' + batches[-1].name)

    def keys(h, name):
        rs = [r for r in places['move'][h] if r['move_key']]
        return sum(digest(r['code'].upper() + '_' + name) == int(r['move_key'], 16) for r in rs), len(rs)

    def rank(source_set):
        return min(ORDER.index(s.split(':')[0]) for s in source_set)

    picked, conflicts, stats = {}, [], co.Counter()
    for (ns, h), names in sorted(cand.items()):
        info, placeholder = {}, ''
        for name, src in names.items():
            if src == {'corpus'} and PLACEHOLDER.match(name) and digest(name) != h:
                placeholder = name; continue
            ok = digest(name) == h
            agree, total = keys(h, name) if ns == 'move' else ('', '')
            info[name] = dict(src=src, ok=ok, agree=agree, total=total, rank=rank(src))
        promoted = {n for n, i in info.items() if ns == 'move' and i['ok'] and i['rank'] == ORDER.index('candidate')
                    and i['total'] and i['agree'] == i['total'] and judged.get((h, n), (0,))[0] == 1}
        usable = [n for n, i in info.items() if i['ok'] and (i['rank'] < ORDER.index('candidate') or n in promoted)]
        if ns == 'move':
            usable.sort(key=lambda n: (-info[n]['agree'], info[n]['rank'], n))
        else:
            usable.sort(key=lambda n: (info[n]['rank'], n))
        pick = usable[0] if usable else ''
        if pick or placeholder:
            picked[(ns, h)] = pick or placeholder
        if placeholder:
            stats['%s placeholders %s' % (ns, 'replaced by a name' if pick else 'kept')] += 1
        if not info:
            continue
        # what kind of disagreement, if any
        kinds = []
        rehashing = [n for n, i in info.items() if i['ok']]
        if len(rehashing) > 1: kinds.append('collision: %d names re-hash to this hash' % len(rehashing))
        if len(info) > len(rehashing): kinds.append('a listed name does not re-hash')
        if len(info) > 1 and not kinds: kinds.append('conflict')
        if ns == 'move' and pick and info[pick]['total'] and info[pick]['agree'] < info[pick]['total']:
            kinds.append('picked name fails %d of %d move keys' % (info[pick]['total'] - info[pick]['agree'], info[pick]['total']))
        if not pick: kinds.append('no name picked')
        if pick and judged.get((h, pick), (1,))[0] != 1: kinds.append('the judge prefers the other name')
        if len(info) == 1 and not kinds:
            continue
        if ns == 'move' and pick:
            i = info[pick]
            others = [info[n]['agree'] for n in usable[1:]]
            reason = ('every move key agrees (%s of %s) and the judge ranks it first; unsettled candidate, picked for the dictionary only'
                      % (i['agree'], i['total']) if pick in promoted
                      else 'most move keys agree (%s of %s)' % (i['agree'], i['total']) if others and i['agree'] > max(others)
                      else 'source order (%s); move keys do not separate the names' % ORDER[i['rank']] if others
                      else 'only name that re-hashes and is not an unsettled candidate')
        elif pick:
            reason = 'source order (%s); animations have no key' % ORDER[info[pick]['rank']] if len(usable) > 1 else 'only name that re-hashes'
        else:
            reason = 'only unsettled candidates or names that do not re-hash'
        stats['hashes with a recorded disagreement, ' + ns] += 1
        codes = sorted({r['code'] for r in places[ns][h]})
        for name, i in sorted(info.items(), key=lambda kv: (kv[1]['rank'], kv[0])):
            conflicts.append(dict(namespace=ns, hash='0x%08X' % h, candidate_name=name, picked='yes' if name == pick else 'no',
                                  pick_reason=reason if name == pick else '', kind='; '.join(kinds), sources=' | '.join(sorted(i['src'])),
                                  rehashes='yes' if i['ok'] else 'no', move_keys_agreeing=i['agree'], move_keys_of_the_hash=i['total'],
                                  judge_rank=judged.get((h, name), ('', ''))[0] if ns == 'move' else '',
                                  judge_score=judged.get((h, name), ('', ''))[1] if ns == 'move' else '',
                                  candidates_for_hash=len(info), rows=len(places[ns][h]), codes=';'.join(codes)))

    # a hash used by both namespaces must end with one name (TKMovesets2 keeps one dictionary)
    for h in sorted(set(places['move']) & set(places['animation'])):
        m, an = picked.get(('move', h)), picked.get(('animation', h))
        if m and an and m != an:
            stats['hashes named differently as move and as animation'] += 1
            for ns, name in (('move', m), ('animation', an)):
                conflicts.append(dict(namespace='both', hash='0x%08X' % h, candidate_name=name, picked='yes' if ns == 'move' else 'no',
                                      pick_reason='one dictionary: the move name is kept' if ns == 'move' else '',
                                      kind='the hash is a move name and an animation name with different picked names',
                                      sources='picked for ' + ns, rehashes='yes', move_keys_agreeing='', move_keys_of_the_hash='',
                                      candidates_for_hash=2, rows=len(places[ns][h]), codes=''))
            picked[('animation', h)] = m

    out_rows = []
    for r in rows:
        r = dict(r)
        for ns, hc, nc in (('move', 'name_hash', 'name'), ('animation', 'anim_hash', 'anim_name')):
            new = picked.get((ns, int(r[hc], 16)), '')
            if new != r[nc]:
                stats['%s rows %s' % (ns, 'filled' if not r[nc] else 'renamed' if new else 'emptied')] += 1
                r[nc] = new
        out_rows.append(r)

    in_rows = set(places['move']) | set(places['animation'])
    extra = {}
    for h, name in sorted(public.items()):
        if h in in_rows: continue
        if digest(name) == h: extra['0x%08X' % h] = name
        else:
            stats['public names of hashes in no row that do not re-hash (left out)'] += 1
            conflicts.append(dict(namespace='not in any row', hash='0x%08X' % h, candidate_name=name, picked='no', pick_reason='',
                                  kind='public name does not re-hash; left out', sources='public', rehashes='no',
                                  move_keys_agreeing='', move_keys_of_the_hash='', candidates_for_hash=1, rows=0, codes=''))

    a.out.mkdir(parents=True, exist_ok=True)
    with (a.out/'ALL_movenames_retail.csv').open('w', encoding='utf-8', newline='') as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator='\n'); w.writeheader(); w.writerows(out_rows)
    (a.out/'extra_names.json').write_text(json.dumps(extra, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')
    columns = ['namespace', 'hash', 'candidate_name', 'picked', 'pick_reason', 'kind', 'sources', 'rehashes', 'move_keys_agreeing',
               'move_keys_of_the_hash', 'judge_rank', 'judge_score', 'candidates_for_hash', 'rows', 'codes']
    with (a.out/'name_conflicts.csv').open('w', encoding='utf-8', newline='') as f:
        w = csv.DictWriter(f, fieldnames=columns, lineterminator='\n', restval=''); w.writeheader(); w.writerows(conflicts)
    named = lambda rs, c: len({int(r[c.replace('name', 'hash') if c == 'anim_name' else 'name_hash'], 16) for r in rs if r[c]})
    report = dict(inputs=inputs, statistics=dict(sorted(stats.items())),
                  rows=len(rows),
                  move_hashes=len(places['move']), move_hashes_named_before=named(rows, 'name'), move_hashes_named_after=named(out_rows, 'name'),
                  animation_hashes=len(places['animation']), animation_hashes_named_before=named(rows, 'anim_name'),
                  animation_hashes_named_after=named(out_rows, 'anim_name'),
                  unnamed_move_rows_before=sum(not r['name'] for r in rows), unnamed_move_rows_after=sum(not r['name'] for r in out_rows),
                  public_entries=len(public), extra_names=len(extra), conflict_lines=len(conflicts),
                  conflict_kinds=dict(co.Counter(c['kind'] for c in conflicts if c['picked'] == 'yes' or c['candidates_for_hash'] == 1).most_common()))
    (a.out/'merge_report.json').write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'inputs'}, indent=1))


if __name__ == '__main__':
    main()
