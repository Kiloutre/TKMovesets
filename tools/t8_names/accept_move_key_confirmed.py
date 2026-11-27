"""Accept key-confirmed move names under the owner's standing rule of 2026-11-17 ("Yes, auto-accept").

Usage: accept_move_key_confirmed.py <repository root> [--write]

Without --write it only prints what it would do. With --write it creates the next batch file
accepted_moves_overlay_move_key_batch<k>.csv (+ .json) next to the masters, and
label_corrections_moves_move_key_batch<k>.csv for key-confirmed names of hashes whose corpus label is wrong.
It never overwrites a file and never edits a master, an earlier overlay or the typed corpus.

Rule (as put to the owner before the answer): a name is recorded once it re-hashes and every move key of its rows agrees.
Exception: a name whose hash has a single key and that comes from one of the large keyed searches stays a candidate
unless the row context also supports it. Applied here as:

  A  two or more keys, all agreeing.
  B  one key, agreeing, and the name comes from a small search: a stored list of an earlier search, or a small edit of
     a name that is itself accepted.
  C  one key, agreeing, from a large keyed search, and the row context supports it: a named move within three rows
     starts with the same head (text up to the first underscore, three characters or more), or the name shares a part
     with the animation name of its row, or a name of another hash within three rows differs from it by one character
     or by one or two appended characters and is itself key-confirmed; and the producing session's estimate of chance
     names that look as good is below 0.2.
  Anything else stays a candidate.

Everything is recomputed here from <workspace>/game/movenames/<code>.csv and the typed corpus; from the producing
session's file (method_lab/keyed_confirmed_all.csv) only the names, the source of each name and the chance estimate
are taken.
"""
import t8names_paths as _P
import collections as co
import csv
import gzip
import hashlib
import json
import sys
from pathlib import Path

WRITE = '--write' in sys.argv[1:]
REC = _P.RECOVERY
LAB = REC/'method_lab'
MOVES = _P.MOVENAMES
from recover import MASKS, digest

ACCEPTED_ON = __import__('datetime').date.today().isoformat()     # batches 2 and 3 were written on 2026-11-26 but carry 2026-11-17 (the date of the rule) in this field
ACCEPTED_BY = 'acceptance rule of tools/t8_names (accept_move_key_confirmed.py)'
DECISION = 'Standing rule: a move name is recorded when the move key confirms it (classes A, B, C below).'
RULE = ('A name is recorded once it re-hashes and every move key of its rows agrees (move_key = Kamui(CODE + "_" + name), retail move record 0xF0). '
        'Class A: two or more keys. Class B: one key and a small search (a stored list of an earlier search, or a small edit of an accepted name). '
        'Class C: one key, a large keyed search, and row context (a named move within three rows starts with the same head, or the name shares a part '
        'with the animation name of its row, or a key-confirmed name of another hash within three rows is one edit away), with the producing '
        'session\'s estimate of equally good chance names below 0.2. A one-key name of a large search without row context stays a candidate.')
MASTERS = {}     # checksums of the master files at the start of the run; they must not change during it
FIELDS = ['namespace', 'hash', 'name', 'status', 'accepted_on', 'accepted_by', 'evidence_class', 'evidence', 'discovery_search', 'candidate_file',
          'decoy_control', 'expected_chance_matches', 'rank_or_statistic', 'anchor', 'codes', 't8_rows', 'name_length', 'rehash_verified',
          'other_namespace_labels_on_rows', 'adjacent_accepted_row', 'note']
CORRECTION_FIELDS = ['hash', 'corpus_label', 'corrected_name', 'status', 'recorded_on', 'evidence_class', 'keys_agreeing', 'keys_of_the_hash',
                     'keys_agreeing_with_the_corpus_label', 'codes', 't8_rows', 'discovery_search', 'candidate_file', 'note']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def rows_of(path):
    with path.open(encoding='utf-8', newline='') as handle:
        return list(csv.DictReader(handle))


for name in ('accepted.csv', 'remaining_unresolved.csv'):
    if (REC/name).exists():
        MASTERS[name] = sha(REC/name)

# keys, read independently of the producing session's move_key.py
by_hash = co.defaultdict(list)
for path in sorted(MOVES.glob('*.csv')):
    if len(path.stem) == 3:
        for r in rows_of(path):
            if r['name_hash'] and r['move_key']:
                by_hash[int(r['name_hash'], 16)].append((path.stem, int(r['id']), int(r['move_key'], 16)))
CODES = sorted({c for v in by_hash.values() for c, _, _ in v})

table = co.defaultdict(dict); verified = {}
with gzip.open(LAB/'typed_corpus'/'rows.jsonl.gz', 'rt', encoding='utf-8') as handle:
    for line in handle:
        r = json.loads(line)
        if r['game'] != 'T8':
            continue
        table[r['code']][int(r['id'])] = r
        if r['move_status'] == 'verified' and r['move_name']:
            verified[int(r['move_hash'], 16)] = r['move_name']
targets = {int(t['hash'], 16) for t in json.loads((LAB/'typed_corpus'/'unresolved_move_targets.json').read_text(encoding='utf-8'))}
earlier = {}; accepted = {}; accepted_anim = {}
for path in sorted(REC.glob('accepted_moves_overlay*.csv')):
    earlier[path.name] = sha(path)
    for r in rows_of(path):
        assert int(r['hash'], 16) not in accepted, 'hash in two overlays: ' + r['hash']
        accepted[int(r['hash'], 16)] = r['name']
for path in [p for p in [REC/'accepted.csv'] if p.exists()] + sorted(REC.glob('accepted_animations_overlay*.csv')):
    for r in rows_of(path):
        accepted_anim[int(r['hash'], 16)] = r['name']
assert set(accepted) <= targets
used_names = set(verified.values()) | set(accepted.values())
batch = 1 + len(list(REC.glob('accepted_moves_overlay_move_key_batch*.csv')))
print('accepted before: %d | open: %d | batch %d | %s' % (len(accepted), len(targets) - len(accepted), batch, 'WRITE' if WRITE else 'dry run'))


def keys_of(value, name):
    places = by_hash.get(value, [])
    return sum(digest(c.upper() + '_' + name) == k for c, _, k in places), len(places)


def wrong_code(value, name):
    return sum(digest(c.upper() + '_' + name) == k for code, _, k in by_hash.get(value, []) for c in CODES if c != code)


def move_label(r, extra):
    if r['move_status'] == 'verified' and r['move_name']:
        return r['move_name']
    value = int(r['move_hash'], 16) if r.get('move_hash') else None
    return accepted.get(value) or extra.get(value) or ''


def anim_label(r):
    if r.get('anim_name'):
        return r['anim_name']
    value = int(r['anim_hash'], 16) if r.get('anim_hash') else None
    return accepted_anim.get(value, '')


def one_edit(a, b):
    if a == b:
        return False
    if len(a) == len(b):
        return sum(x != y for x, y in zip(a, b)) == 1
    short, long_ = (a, b) if len(a) < len(b) else (b, a)
    return len(long_) - len(short) <= 2 and long_.startswith(short)


def context(value, name, named, confirmed):
    """named: hash -> name usable as a named neighbour; confirmed: hash -> key-confirmed name of this batch (for the sibling test)."""
    head = name.split('_')[0] + '_' if '_' in name else ''
    parts = [t.lower() for t in name.split('_') if len(t) >= 3]
    like = None; shared = set(); sibling = None
    for code, i, _ in by_hash.get(value, []):
        row = table[code].get(i)
        if row:
            anim = [t.lower() for t in anim_label(row).split('_') if t]
            shared |= {t for t in parts if t in anim}
        for j in range(i - 3, i + 4):
            q = table[code].get(j)
            if not q or j == i:
                continue
            known = move_label(q, named)
            if known and len(head) >= 3 and known.startswith(head) and like is None:
                like = '%s:%d %s' % (code, j, known)
            other = int(q['move_hash'], 16) if q.get('move_hash') else None
            partner = confirmed.get(other) or move_label(q, {})
            if other != value and partner and one_edit(name, partner) and sibling is None:
                sibling = '%s:%d %s' % (code, j, partner)
    return like, sorted(shared), sibling


source = rows_of(LAB/'keyed_confirmed_all.csv')
stored = {(int(r['hash'], 16), r['name']) for r in rows_of(LAB/'key_confirmed_names.csv')} if (LAB/'key_confirmed_names.csv').exists() else set()     # optional: names of earlier small searches
cands = {}; labels = {}
for r in source:
    if r['verdict'] != 'real':
        continue
    value = int(r['hash'], 16)
    if r['group'] == 'open':
        if value in accepted:
            assert accepted[value] == r['name'], 'conflict with an accepted name: ' + r['name']
            continue
        assert value not in cands, 'two names taken as real for ' + r['hash']
        cands[value] = r
    else:
        assert value not in labels, 'two corrections for ' + r['hash']
        labels[value] = r

# checks every name must pass, whatever its class
for value, r in list(cands.items()) + list(labels.items()):
    name = r['name']
    a, n = keys_of(value, name)
    assert digest(name) == value, name + ' does not re-hash'
    assert n >= 1 and a == n, '%s: keys %d of %d' % (name, a, n)
    assert wrong_code(value, name) == 0, name + ' agrees with the key of another list code'
    assert not any(m and digest(name) ^ m == value for m in MASKS)
    assert name not in used_names, name + ' is already the name of another hash'
for value in cands:
    assert value in targets and value not in verified
for value, r in labels.items():
    assert value in verified and verified[value] == r['label'] and value not in targets
    assert keys_of(value, verified[value])[0] == 0, 'the corpus label agrees with a key: ' + r['label']
assert len({r['name'] for r in cands.values()}) == len(cands)


def classify(pool):
    """pool: hash -> source row. Returns hash -> (class letter, reason, context tuple)."""
    confirmed = {v: r['name'] for v, r in pool.items()}
    result = {}
    for value, r in pool.items():
        if keys_of(value, r['name'])[1] >= 2:
            result[value] = ('A', 'two or more keys', None)
    changed = True
    while changed:
        changed = False
        safe = {v: pool[v]['name'] for v in result}
        for value, r in pool.items():
            if value in result:
                continue
            name = r['name']; src = r['sources']
            if 'stored lists' in src:
                assert (value, name) in stored, name + ' is not in key_confirmed_names.csv'
                result[value] = ('B', 'one key; in a stored list of an earlier search', None); changed = True
                continue
            if 'small edit of `' in src:
                parent = src.split('small edit of `', 1)[1].split('`', 1)[0]
                if parent in set(safe.values()) | used_names:
                    result[value] = ('B', 'one key; small edit of the accepted name %s' % parent, None); changed = True
                    continue
            like, shared, sibling = context(value, name, safe, confirmed)
            chance = float(r['chance']) if r['chance'] else 0.0
            if (like or shared or sibling) and chance < 0.2:
                why = []
                if like:
                    why.append('named neighbour with the same head: ' + like)
                if shared:
                    why.append('shares %s with the animation name of its row' % ', '.join(shared))
                if sibling:
                    why.append('one edit from the key-confirmed name on ' + sibling)
                result[value] = ('C', 'one key; large keyed search; ' + '; '.join(why), (like, shared, sibling)); changed = True
    return result


classes = classify(cands)
label_classes = classify(labels)
left = sorted((cands[v]['name'], v) for v in cands if v not in classes)
print('names taken as real by the producing session for open hashes and not yet accepted: %d' % len(cands))
print('by class:', dict(co.Counter(c for c, _, _ in classes.values())), '| left as candidates:', len(left))
for name, value in left:
    r = cands[value]
    print('   LEFT  %08X %-22s one key; %s; chance %s; rows %s' % (value, name, r['sources'][:50], r['chance'] or '-', r['rows'][:30]))
chance_c = sum(float(cands[v]['chance'] or 0) for v, (c, _, _) in classes.items() if c == 'C')
print('class C: sum of the producing session\'s chance estimates: %.4f' % chance_c)

named_all = dict(accepted); named_all.update({v: cands[v]['name'] for v in classes})
rows = []
for value in sorted(classes, key=lambda v: cands[v]['name']):
    r = cands[value]; name = r['name']; letter, reason, _ = classes[value]
    a, n = keys_of(value, name)
    places = by_hash[value]
    codes = sorted({c for c, _, _ in places})
    anims = sorted({'%s:%d %s' % (c, i, anim_label(table[c][i])) for c, i, _ in places if table[c].get(i) and anim_label(table[c][i])})
    code, i, _ = places[0]
    near = []
    for j in (i - 1, i + 1):
        q = table[code].get(j)
        if q and move_label(q, named_all):
            near.append('%s:%d %s' % (code, j, move_label(q, named_all)))
    like, shared, sibling = context(value, name, named_all, {v: cands[v]['name'] for v in classes})
    evidence = 'Re-hashes; %d of %d move keys agree (%s); the key of no other list code agrees.' % (
        a, n, ', '.join('%s:%d' % (c, k) for c, k, _ in places[:4]) + (' and %d more rows' % (n - 4) if n > 4 else ''))
    evidence += ' ' + reason[0].upper() + reason[1:] + '.'
    rows.append(dict(namespace='move', hash='0x%08X' % value, name=name, status='accepted', accepted_on=ACCEPTED_ON, accepted_by=ACCEPTED_BY,
                     evidence_class={'A': 'move key: two or more keys agree', 'B': 'move key: one key agrees, small search',
                                     'C': 'move key: one key agrees, large keyed search, row context'}[letter],
                     evidence=evidence, discovery_search=r['sources'], candidate_file='method_lab/keyed_confirmed_all.csv',
                     decoy_control='key of each of the %d other list codes: 0 agree; XOR masks 13371337, 5A5A5A5A, A5A5A5A5, 0F0F0F0F: none maps the name onto the hash' % (len(CODES) - 1),
                     expected_chance_matches={'A': 'below 2^-32 for anything the searches cover', 'B': 'negligible: fewer than 2^40 names tried per hash against 64 bits',
                                              'C': 'producing session: %s chance names expected to look as good' % (r['chance'] or '0')}[letter],
                     rank_or_statistic=r['reading'], anchor=(sibling or like or ''), codes=';'.join(codes),
                     t8_rows='|'.join('%s:%d' % (c, k) for c, k, _ in places), name_length=len(name), rehash_verified='yes',
                     other_namespace_labels_on_rows=' ; '.join(anims[:6]), adjacent_accepted_row=' ; '.join(near),
                     note=r['context']))
assert len(rows) == len(classes) and not ({int(x['hash'], 16) for x in rows} & set(accepted))

corrections = []
for value in sorted(labels, key=lambda v: labels[v]['name']):
    r = labels[value]; a, n = keys_of(value, r['name'])
    places = by_hash[value]
    letter = label_classes.get(value, ('', 'one key; large search; no row context', None))
    letter = (letter[0], letter[1].replace('large keyed search', 'large search'), letter[2])   # some corrections come from the complete short searches
    corrections.append(dict(hash='0x%08X' % value, corpus_label=r['label'], corrected_name=r['name'],
                            status='correction recorded; typed corpus not edited' if value in label_classes else 'candidate correction, not recorded as settled',
                            recorded_on=ACCEPTED_ON, evidence_class=(letter[0] + ': ' if letter[0] else '') + letter[1], keys_agreeing=a, keys_of_the_hash=n,
                            keys_agreeing_with_the_corpus_label=0, codes=';'.join(sorted({c for c, _, _ in places})),
                            t8_rows='|'.join('%s:%d' % (c, k) for c, k, _ in places), discovery_search=r['sources'],
                            candidate_file='method_lab/keyed_confirmed_all.csv', note=r['context']))
for i in range(22, 36):
    old, new = 'sGrd_LUR4Sid%d' % i, 'sGrd_hvy_mid%d' % i
    value = digest(old)
    if verified.get(value) != old:
        continue      # already corrected in the name table this workspace was built from
    assert digest(new) == value
    a, n = keys_of(value, new)
    assert a == n >= 1 and keys_of(value, old)[0] == 0 and wrong_code(value, new) == 0
    places = by_hash[value]
    corrections.append(dict(hash='0x%08X' % value, corpus_label=old, corrected_name=new, status='correction recorded; typed corpus not edited', recorded_on=ACCEPTED_ON,
                            evidence_class=('A: two or more keys' if n >= 2 else 'B: one key; name from the series found by the paired-hash search'),
                            keys_agreeing=a, keys_of_the_hash=n, keys_agreeing_with_the_corpus_label=0, codes=';'.join(sorted({c for c, _, _ in places})),
                            t8_rows='|'.join('%s:%d' % (c, k) for c, k, _ in places), discovery_search='paired-hash search (collision series), 2026-11-17',
                            candidate_file='method_lab/sibling_search_findings.md', note='same hash as the label: a true collision; the keys decide'))
recorded = {}
for path in sorted(REC.glob('label_corrections_moves_move_key_batch*.csv')):
    for c in rows_of(path):
        recorded[c['hash']] = (c['corrected_name'], c['status'])
all_corrections = len(corrections)
corrections = [c for c in corrections if recorded.get(c['hash']) != (c['corrected_name'], c['status'])]   # only what earlier batches do not hold
print('label corrections: %d known, %d new or changed (%d of those settled)' % (all_corrections, len(corrections),
      sum(c['status'].startswith('correction recorded') for c in corrections)))
for c in corrections:
    if not c['status'].startswith('correction recorded'):
        print('   LEFT  %s %s -> %s (%s)' % (c['hash'], c['corpus_label'], c['corrected_name'], c['evidence_class']))

for letter in 'ABC':
    print('== class %s' % letter)
    for value in sorted(classes, key=lambda v: cands[v]['name']):
        if classes[value][0] == letter:
            a, n = keys_of(value, cands[value]['name'])
            print('   %08X %-24s keys %2d of %2d  %s' % (value, cands[value]['name'], a, n, classes[value][1][:110] if letter != 'A' else cands[value]['rows'][:40]))

if not WRITE:
    sys.exit(0)

stem = 'accepted_moves_overlay_move_key_batch%d' % batch
target = REC/(stem + '.csv'); cfile = REC/('label_corrections_moves_move_key_batch%d.csv' % batch)
assert rows, 'nothing to accept'
for path in (target, REC/(stem + '.json'), cfile):
    assert not path.exists(), 'refusing to overwrite ' + path.name
with target.open('w', encoding='utf-8', newline='') as handle:
    writer = csv.DictWriter(handle, FIELDS); writer.writeheader(); writer.writerows(rows)
if corrections:
    with cfile.open('w', encoding='utf-8', newline='') as handle:
        writer = csv.DictWriter(handle, CORRECTION_FIELDS); writer.writeheader(); writer.writerows(corrections)
for name, expected in MASTERS.items():
    assert sha(REC/name) == expected
for name, value in earlier.items():
    assert sha(REC/name) == value, name + ' changed during the run'
record = dict(schema_version=1, file=target.name, sha256=sha(target), rows=len(rows), namespace='move', accepted_on=ACCEPTED_ON, decision=DECISION,
              rule_applied=RULE, by_class=dict(co.Counter(classes[int(x['hash'], 16)][0] for x in rows)),
              names=[x['name'] for x in rows],
              left_as_candidate=['%s (0x%08X): one key, large keyed search, no row context by the rule; producing session\'s chance estimate %s'
                                 % (name, value, cands[value]['chance'] or '0') for name, value in left],
              class_c_sum_of_chance_estimates=round(chance_c, 5),
              label_corrections_file=cfile.name if corrections else None, label_corrections_sha256=sha(cfile) if corrections else None,
              label_corrections=len(corrections),
              checkpoint=dict(raw_unresolved=len(targets), accepted_before=len(accepted), this_overlay=len(rows),
                              accepted_total=len(accepted) + len(rows), still_without_accepted_name=len(targets) - len(accepted) - len(rows)),
              every_name_rehashed=True, every_key_of_every_name_agrees=True, no_key_of_another_list_code_agrees=True,
              no_hash_shared_with_earlier_files=True, no_name_is_a_verified_or_accepted_name=True, no_name_matches_a_decoy_mask=True,
              masters_unchanged=MASTERS, earlier_files_unchanged=earlier, source_file='method_lab/keyed_confirmed_all.csv',
              source_file_sha256=sha(LAB/'keyed_confirmed_all.csv'),
              policy='Overlay only; the masters and the typed corpus are not edited. A session that needs accepted labels reads every '
                     'accepted_moves_overlay*.csv file. The label corrections are in their own file and are not part of the raw unresolved hashes.')
(REC/(stem + '.json')).write_text(json.dumps(record, indent=1) + '\n', encoding='utf-8')
print(json.dumps({k: record[k] for k in ('file', 'sha256', 'rows', 'by_class', 'left_as_candidate', 'class_c_sum_of_chance_estimates', 'label_corrections', 'checkpoint')}, indent=1))
