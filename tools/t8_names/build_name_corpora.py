"""Build immutable-source, typed local corpora and corrected overlays only."""
import t8names_paths as _P
import argparse
import collections as co
import csv
import gzip
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import re
from name_corpora import HERE, DEFAULT_DIRECTORY, PLACEHOLDER, is_label

ROOT = None  # locations come from t8names_paths
HANDOFF = HERE.parent.parent
T7_ROOT = _P.LEGACY
T8_SOURCE = _P.MOVENAMES/'ALL_movenames_retail.csv'
REVIEW_PATTERN = r'mod|old|TAG2|CHUN_LI|MR\.X|GILL|VAMPIRE|NEGANE?V?2|FRV_n'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def readcsv(path):
    with path.open(encoding='utf-8-sig', newline='') as f:
        return list(csv.DictReader(f))


def hash_text(value):
    if value is None or value == '':
        return None
    integer = int(value, 16) if isinstance(value, str) else value
    if not 0 <= integer <= 0xffffffff:
        raise ValueError('source hash outside uint32')
    return f'0x{integer:08X}'


def writejson(path, value):
    encoded = (json.dumps(value, ensure_ascii=True, indent=2, sort_keys=True) + '\n').encode('utf-8')
    temporary = path.with_suffix(path.suffix + '.tmp')
    temporary.write_bytes(encoded)
    temporary.replace(path)


def build(directory=DEFAULT_DIRECTORY, t7_root=T7_ROOT, t8_source=T8_SOURCE):
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    inputs = []

    def record(path, role):
        raw = path.read_bytes()
        inputs.append(dict(path=str(path.resolve()), role=role, bytes=len(raw), sha256=sha(raw)))
        return raw

    hashpath = _P.SRC / 'kamuiHash.py'
    record(hashpath, 'hash_reference')
    spec = importlib.util.spec_from_file_location('_typed_corpus_kamui', hashpath)
    hashing = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(hashing)
    cache = {}

    def calculated(name):
        if name not in cache:
            cache[name] = f'0x{hashing.getKamuiHashDigest(name):08X}' if name.isascii() else None
        return cache[name]

    sources = []
    rows = []
    raw_counts = co.Counter()
    for path in sorted(Path(t7_root).glob('t7_*/*.json')):
        raw = record(path, 'T7_extract')
        data = json.loads(raw)
        flags = []
        if data.get('original_hash') != data.get('last_calculated_hash'):
            flags.append('stored_hashes_differ')
        if re.search(REVIEW_PATTERN, path.stem, re.I):
            flags.append('label_needs_variant_review')
        source_id = 'T7/' + path.relative_to(t7_root).as_posix()
        code = data.get('character_name') or path.stem
        source = dict(source_id=source_id, game='T7', path=str(path.resolve()), sha256=sha(raw),
                      code=code, flags=flags, rows=len(data.get('moves', [])))
        for key in ('version', 'export_version', 'character_id', 'character_name', 'tekken_character_name',
                    'extraction_date', 'original_hash', 'last_calculated_hash', 'creator_name'):
            source[key] = data.get(key)
        sources.append(source)
        for i, row in enumerate(data.get('moves', [])):
            rows.append(dict(game='T7', source_id=source_id, source_flags=flags, code=code, id=i,
                             source_rows=[i], move_name=row.get('name') or '', anim_name=row.get('anim_name') or '',
                             move_hash=None, anim_hash=None))
            raw_counts['T7'] += 1
    raw = record(Path(t8_source), 'T8_retail_export')
    t8_source_id = 'T8/ALL_movenames_retail.csv'
    exported = list(csv.DictReader(io.StringIO(raw.decode('utf-8-sig'))))
    sources.append(dict(source_id=t8_source_id, game='T8', path=str(Path(t8_source).resolve()),
                        sha256=sha(raw), flags=[], rows=len(exported)))
    for i, row in enumerate(exported):
        rows.append(dict(game='T8', source_id=t8_source_id, source_flags=[], code=row['code'], id=int(row['id']),
                         source_rows=[i], move_name=row.get('name') or '', anim_name=row.get('anim_name') or '',
                         move_hash=hash_text(row.get('name_hash')), anim_hash=hash_text(row.get('anim_hash'))))
        raw_counts['T8'] += 1

    # A source row number is evidence of duplicates, not a reason to keep the
    # same provenance/name/hash record twice. Every original row index survives.
    unique = {}
    for row in rows:
        identity = tuple(row[k] for k in ('game', 'source_id', 'code', 'id', 'move_name', 'anim_name', 'move_hash', 'anim_hash'))
        if identity in unique:
            unique[identity]['source_rows'].extend(row['source_rows'])
        else:
            unique[identity] = row
    rows = list(unique.values())
    labels = {}
    fields = (('move', 'move_name', 'move_hash'), ('animation', 'anim_name', 'anim_hash'))
    for row in rows:
        for namespace, field, hashfield in fields:
            name = row[field]
            if not is_label(name):
                continue
            key = row['game'], namespace, name
            label = labels.setdefault(key, dict(game=row['game'], namespace=namespace, name=name,
                computed_kamui_hash=calculated(name), source_hashes=set(), source_ids=set(), codes=set(), rows=0))
            if row[hashfield] is not None:
                label['source_hashes'].add(row[hashfield])
            label['source_ids'].add(row['source_id'])
            label['codes'].add(row['code'])
            label['rows'] += 1
    for label in labels.values():
        if label['computed_kamui_hash'] is None:
            label['status'] = 'non_ascii'
        elif label['game'] == 'T7':
            label['status'] = 'source_unverified'
        elif label['source_hashes'] == {label['computed_kamui_hash']}:
            label['status'] = 'verified'
        else:
            label['status'] = 'hash_conflict' if len(label['source_hashes']) > 1 else 'hash_mismatch'
        for key in ('source_hashes', 'source_ids', 'codes'):
            label[key] = sorted(label[key])
    for row in rows:
        for namespace, field, _ in fields:
            name = row[field]
            status = labels[row['game'], namespace, name]['status'] if is_label(name) else ('placeholder' if name else 'empty')
            row['move_status' if namespace == 'move' else 'anim_status'] = status

    frozen = {}
    for filename, path in (
        ('known_anim_names.csv', HANDOFF / 'known_anim_names.csv'),
        ('known_move_to_anim.csv', HANDOFF / 'known_move_to_anim.csv'),
        ('unresolved_anim_hashes.csv', HANDOFF / 'unresolved_anim_hashes.csv'),
        ('accepted.csv', HERE.parent / 'accepted.csv'),
        ('remaining_unresolved.csv', HERE.parent / 'remaining_unresolved.csv')):
        if path.exists():
            record(path, 'frozen_handoff_or_recovery_master')
            frozen[filename] = readcsv(path)
    accepted = {hash_text(r['hash']): r['name'] for r in frozen.get('accepted.csv', [])}
    targets = {}
    for namespace, field, hashfield in fields:
        groups = {}
        statusfield = 'move_status' if namespace == 'move' else 'anim_status'
        for row in rows:
            if row['game'] != 'T8' or row[statusfield] == 'verified':
                continue
            key = row[hashfield]
            if key is None:
                continue
            target = groups.setdefault(key, dict(namespace=namespace, hash=key, rows=[], codes=set(),
                supplied_labels=set(), known_counterpart_names=set(), source_statuses=set()))
            target['codes'].add(row['code'])
            if row[field]:
                target['supplied_labels'].add(row[field])
            otherfield, otherstatus = ('anim_name', 'anim_status') if namespace == 'move' else ('move_name', 'move_status')
            if row[otherstatus] == 'verified':
                target['known_counterpart_names'].add(row[otherfield])
            target['source_statuses'].add(row[statusfield])
            target['rows'].append(dict(source_id=row['source_id'], code=row['code'], id=row['id'],
                move_name=row['move_name'] if row['move_status'] == 'verified' else '',
                anim_name=row['anim_name'] if row['anim_status'] == 'verified' else '',
                move_hash=row['move_hash'], anim_hash=row['anim_hash'], source_rows=row['source_rows']))
        for target in groups.values():
            for key in ('codes', 'supplied_labels', 'known_counterpart_names', 'source_statuses'):
                target[key] = sorted(target[key])
            target['uses'] = len(target['rows'])
            target['placeholder_lengths'] = sorted({len(n) for n in target['supplied_labels'] if PLACEHOLDER.fullmatch(n)})
            target['status'] = 'unresolved_in_raw_export'
            if namespace == 'animation' and target['hash'] in accepted:
                target['accepted_recovery_name'] = accepted[target['hash']]
                target['status'] = 'raw_export_unresolved_but_recovery_master_accepted'
        targets[namespace] = sorted(groups.values(), key=lambda r: r['hash'])

    existing_names = {r['anim_name'] for r in frozen.get('known_anim_names.csv', [])}
    extra = [r for r in labels.values() if r['game'] == 'T8' and r['namespace'] == 'animation'
             and r['status'] == 'verified' and r['name'] not in existing_names]
    extra_names = {r['name'] for r in extra}
    mappings = [dict(code=r['code'], move_id=r['id'], move_name=r['move_name'] if r['move_status'] == 'verified' else '',
                     anim_name=r['anim_name'], anim_hash=r['anim_hash'], source_id=r['source_id'], source_rows=r['source_rows'])
                for r in rows if r['game'] == 'T8' and r['anim_name'] in extra_names]
    raw_unresolved = {hash_text(r['anim_hash']) for r in frozen.get('unresolved_anim_hashes.csv', [])}
    overlay = dict(interpretation='Verified corrections to original handoff filtering. Apply only as a separate overlay; originals and recovery masters are unchanged.',
        animation_name_additions=[dict(anim_name=r['name'], hash=r['computed_kamui_hash'], uses=r['rows'], codes=r['codes']) for r in sorted(extra, key=lambda r:r['name'])],
        move_to_animation_additions=mappings,
        superseded_original_unresolved_hashes=sorted({r['computed_kamui_hash'] for r in extra} & raw_unresolved),
        accepted_recoveries_included_in_training=False)

    label_list = [labels[k] for k in sorted(labels)]
    counts = {}
    for game in ('T7', 'T8'):
        for namespace in ('move', 'animation'):
            ll = [r for r in label_list if r['game'] == game and r['namespace'] == namespace]
            unflagged_ids = {r['source_id'] for r in sources if not r['flags']}
            counts[f'{game}_{namespace}'] = dict(unique_labels=len(ll), statuses=dict(co.Counter(r['status'] for r in ll)),
                unique_labels_with_unflagged_source=sum(bool(set(r['source_ids']) & unflagged_ids) for r in ll))

    writejson(directory / 'sources.json', sources)
    writejson(directory / 'labels.json', label_list)
    writejson(directory / 'corrected_handoff_overlay.json', overlay)
    for namespace in ('move', 'animation'):
        writejson(directory / f'unresolved_{namespace}_targets.json', targets[namespace])
    rowpath = directory / 'rows.jsonl.gz'
    temporary = rowpath.with_suffix('.tmp')
    with temporary.open('wb') as raw:
        with gzip.GzipFile(fileobj=raw, mode='wb', filename='', mtime=0, compresslevel=6) as packed:
            for row in rows:
                packed.write((json.dumps(row, separators=(',', ':'), sort_keys=True) + '\n').encode('ascii'))
    temporary.replace(rowpath)
    for path in (_P.SRC/'audit_name_corpora.py', _P.SRC/'name_corpora.py', _P.SRC/'build_name_corpora.py'):
        record(path, 'audit_or_builder_source')
    output_files = ('sources.json', 'labels.json', 'corrected_handoff_overlay.json', 'unresolved_move_targets.json',
                    'unresolved_animation_targets.json', 'rows.jsonl.gz')
    manifest = dict(schema_version=1, inputs=inputs, source_files=len(sources),
        raw_rows=dict(raw_counts), deduplicated_rows=dict(co.Counter(r['game'] for r in rows)),
        exact_duplicate_rows_removed=sum(raw_counts.values()) - len(rows), type_counts=counts,
        t7_review=dict(files=sum(r['game']=='T7' for r in sources), flagged_files=sum(r['game']=='T7' and bool(r['flags']) for r in sources),
            label_pattern=REVIEW_PATTERN, policies=['none','unflagged','all'],
            caveat='Review flags are heuristic. Unflagged does not mean official or unmodified.'),
        placeholder_regex=PLACEHOLDER.pattern,
        unresolved_target_counts={ns:len(rr) for ns,rr in targets.items()},
        raw_animation_targets_already_accepted=sum('accepted_recovery_name' in t for t in targets['animation']),
        overlay_animation_additions=len(extra),
        outputs={n:dict(bytes=(directory/n).stat().st_size, sha256=sha((directory/n).read_bytes())) for n in output_files},
        interpretation='Source-tagged literal names. T8 labels are verified against every associated source hash; T7 computed Kamui hashes have no T7 source-hash verification. Raw export targets and accepted recovery annotations are separate. No original files were changed.')
    writejson(directory / 'manifest.json', manifest)
    print(json.dumps({k:manifest[k] for k in ('source_files','raw_rows','deduplicated_rows','exact_duplicate_rows_removed','type_counts','unresolved_target_counts','raw_animation_targets_already_accepted','overlay_animation_additions')}, indent=2))
    return manifest


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=DEFAULT_DIRECTORY)
    parser.add_argument('--t7-root', type=Path, default=T7_ROOT)
    parser.add_argument('--t8-source', type=Path, default=T8_SOURCE)
    args = parser.parse_args()
    build(args.output, args.t7_root, args.t8_source)
