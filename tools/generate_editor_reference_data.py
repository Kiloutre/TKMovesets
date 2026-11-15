"""Generate editor labels and review-only T7 -> retail T8 mapping candidates.

No candidate in this file authorizes conversion or claims parameter equivalence.
"""
from collections import defaultdict
import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / 'research/moveset-reference-sheets'


def normalized(label):
    return ' '.join((label or '').split()).casefold()


def labels(entries, game, category, legacy=None):
    result = dict(legacy or {})
    grouped = defaultdict(list)
    for entry in entries:
        if entry['game'] == game and entry['category'] == category and entry['label']:
            text = entry['label'].replace('\r\n', '\n').replace('\r', '\n')
            if text not in grouped[entry['id']]:
                grouped[entry['id']].append(text)
    for identifier, values in grouped.items():
        result[str(identifier)] = ' / '.join(values).replace('\n', r'\n')
    return ''.join(f'{int(key)}, {value}\n' for key, value in sorted(result.items(), key=lambda pair: int(pair[0])))


def mappings(entries):
    targets = defaultdict(list)
    for entry in entries:
        if entry['game'] == 't8-retail' and normalized(entry['label']):
            targets[entry['category'], normalized(entry['label'])].append(entry)
    result = []
    for source in entries:
        if source['game'] != 't7':
            continue
        matches = targets[source['category'], normalized(source['label'])] if normalized(source['label']) else []
        candidates = [{'id': entry['id'], 'label': entry['label'],
                       'parameter_or_value_range': entry['parameter_or_value_range'],
                       'description': entry['description'], 'notes': entry['notes'], 'source': entry['source']}
                      for entry in matches]
        ids = {entry['id'] for entry in matches}
        status = 'ambiguous' if len(ids) > 1 else 'candidate' if ids else 'unmapped'
        result.append({'category': source['category'], 't7_id': source['id'], 'label': source['label'],
                       'parameter_or_value_range': source['parameter_or_value_range'],
                       'description': source['description'], 'notes': source['notes'], 'source': source['source'],
                       'status': status, 'evidence': 'exact_label_ignoring_case_and_whitespace' if matches else None,
                       'parameter_transform': None, 'runtime_enabled': False, 'retail_candidates': candidates})
    return {'schema_version': 1, 'target_game': 't8-retail', 'target_build': 'unspecified',
            'policy': 'Review candidates only. Validate behavior and parameter transforms before conversion.',
            'entries': result}


def resolve(mapping, decisions):
    index = {}
    for decision in decisions:
        key = decision['category'], decision['t7_id']
        if key in index:
            raise ValueError("Duplicate mapping decision")
        index[key] = decision
    for row in mapping['entries']:
        decision = index.get((row['category'], row['t7_id']))
        if decision:
            if decision['retail_id'] not in {c['id'] for c in row['retail_candidates']}:
                raise ValueError("Decision selects a noncandidate target")
            row['status'] = 'documented_alignment'
            row['selected_retail_id'] = decision['retail_id']
            row['resolution'] = decision
    return mapping


def alias_evidence(mapping, catalog):
    if catalog['target_game'] != 't8-retail' or catalog['target_build'] != '26155785' or catalog['runtime_enabled']:
        raise ValueError('Expected review-only retail 26155785 aliases')
    index = {}
    for row in catalog['entries']:
        key = row['category'], row['t7_id']
        if key in index or row['runtime_enabled'] or row['parameter_transform'] is not None:
            raise ValueError('Duplicate alias or unsupported parameter/runtime authorization')
        index[key] = row
    mapping['build_pinned_alias_catalog'] = {'path': 'research/t7-to-retail-t8-aliases.json',
                                            'target_build': catalog['target_build'], 'runtime_enabled': False}
    for row in mapping['entries']:
        evidence = index.get((row['category'], row['t7_id']))
        if evidence is not None:
            row['offline_alias_evidence'] = {key: evidence[key] for key in
                ('status', 'effective_status', 'cnt_id', 'research_selected_retail_id',
                 'review_retail_id', 'flags', 'parameter_transform', 'runtime_enabled')}
            row['offline_alias_evidence']['target_build'] = catalog['target_build']
    return mapping


def handler_labels(text, catalog, game, category):
    resolution = catalog['handler_resolution']
    if resolution['runtime_enabled'] or resolution['target_steam_build_id'] != catalog['target_build']:
        raise ValueError('Invalid handler-label build or runtime authorization')
    existing = {int(line.partition(', ')[0]): line.partition(', ')[2]
                for line in text.splitlines() if line}
    seen = set()
    for label in resolution['labels']:
        key = label['game'], label['category'], label['id']
        if label['game'] not in ('t7', 't8-retail') or label['category'] != 'extra_property' or key in seen:
            raise ValueError('Invalid handler-label namespace or duplicate')
        seen.add(key)
        if (label['game'], label['category']) == (game, category):
            # These reviewed additions fill missing labels; sheet text is never erased.
            if label['id'] in existing and existing[label['id']] != label['label']:
                raise ValueError('Handler label would overwrite an existing sheet or legacy label')
            existing[label['id']] = label['label']
    return ''.join(f'{key}, {value}\n' for key, value in sorted(existing.items()))


def validate_handler_review(review, evidence):
    if (review['target_game'] != 't8-retail' or review['target_build'] != '26155785'
            or review['runtime_enabled'] or review['parameter_transform'] is not None
            or evidence['retail_build'] != review['target_build'] or evidence['runtime_enabled']):
        raise ValueError('Expected offline retail handler review')
    expected = {(r['category'], r['source_id'], r['target_id']) for r in evidence['requests']}
    requests = {(r['category'], r['source_id'], r['target_id']): r for r in evidence['requests']}
    actual = set()
    for row in review['entries']:
        key = row['category'], row['source_id'], row['candidate_id']
        if key in actual or row['runtime_enabled'] or row['parameter_transform'] is not None:
            raise ValueError('Duplicate handler review or unsupported runtime/transform authorization')
        actual.add(key)
        if key not in requests or any(row['evidence_handlers'].get(role) != reference
                                     for role, reference in requests[key]['handlers'].items()):
            raise ValueError('Handler review does not reference its own source/target evidence')
        expected_selection = (row['candidate_id'] if row['decision'] == 'select_candidate' else
                              row['previous_retail_id'] if row['decision'] in ('retain_previous', 'retain_existing_id')
                              and row['status'] not in ('insufficient_semantic_evidence', 'insufficient_handler_evidence') else None)
        if row['selected_retail_id'] != expected_selection:
            raise ValueError('Handler selection disagrees with its review decision')
        if any(key not in evidence['handlers'] and key not in evidence['t7_decompiles']
               for key in row['evidence_handlers'].values()):
            raise ValueError('Unknown handler evidence reference')
    if actual != expected:
        raise ValueError('Handler review must cover every selected evidence pair')
    seen = set()
    for label in review['labels']:
        row = review['entries'][label['review_index']]
        key = label['category'], label['id']
        category_ok = label['category'] == row['category'] or (
            label['category'] == 'properties' and row['category'] == 'requirements' and row['candidate_id'] >= 0x8000)
        if (label['game'] != 't8-retail' or label['category'] not in ('requirements', 'properties')
                or key in seen or label['runtime_enabled'] or label['mode'] != 'fill_missing'
                or label['id'] != row['candidate_id'] or not category_ok
                or row['status'] not in ('handler_supported_alignment', 'handler_supported_operation', 'difference_corrected')):
            raise ValueError('Label lacks a supported retail handler review')
        seen.add(key)


def reviewed_labels(text, review, category):
    existing = {int(line.partition(', ')[0]): line.partition(', ')[2]
                for line in text.splitlines() if line}
    for label in review['labels']:
        if label['category'] == category:
            if label['id'] in existing and existing[label['id']] != label['label']:
                raise ValueError(f'Reviewed {category} label 0x{label["id"]:04X} would overwrite existing reference text')
            existing[label['id']] = label['label']
    return ''.join(f'{key}, {value}\n' for key, value in sorted(existing.items()))


def code_labels(text, catalog, category):
    """Fills ids that have no label with the names read from the retail handlers; existing text always wins."""
    if (catalog['target_game'] != 't8-retail' or catalog['target_build'] != '26155785'
            or catalog['mode'] != 'fill_missing' or catalog['runtime_enabled']):
        raise ValueError('Expected fill-only retail 26155785 code labels')
    existing = {int(line.partition(', ')[0]): line.partition(', ')[2]
                for line in text.splitlines() if line}
    seen = set()
    for label in catalog['labels']:
        if label['category'] not in ('requirements', 'properties') or (label['category'], label['id']) in seen:
            raise ValueError('Invalid or duplicate code label')
        seen.add((label['category'], label['id']))
        if label['category'] == category and label['id'] not in existing:
            existing[label['id']] = label['label']
    for label in catalog.get('replacements', []):
        if label['category'] != category:
            continue
        current = existing.get(label['id'])
        # Approved replacement of a label the code contradicts: only while the label is still the reviewed one.
        if current not in (label['previous'], label['label']):
            raise ValueError(f'Label {category} {label["id"]} changed since its replacement was approved')
        existing[label['id']] = label['label']
    return ''.join(f'{key}, {value}\n' for key, value in sorted(existing.items()))


def annotate_handler_reviews(mapping, review):
    mapping['handler_review_catalog'] = dict(path='research/retail-t8-handler-review.json',
                                           target_build=review['target_build'], runtime_enabled=False)
    for row in mapping['entries']:
        category = 'requirements' if row['category'] == 'requirement' else 'properties'
        matches = [dict(review_index=i, candidate_id=r['candidate_id'], status=r['status'], decision=r['decision'],
                        selected_retail_id=r['selected_retail_id'])
                   for i, r in enumerate(review['entries'])
                   if r['category'] == category and r['source_id'] == row['t7_id']]
        if matches:
            row['offline_handler_reviews'] = matches
    return mapping


def outputs(root=ROOT):
    reference = root / 'research/moveset-reference-sheets'
    entries = json.loads((reference / 'label-catalog.json').read_text(encoding='utf-8'))['entries']
    legacy = json.loads((reference / 'legacy-t7-labels.json').read_text(encoding='utf-8'))
    aliases = json.loads((root / 'research/t7-to-retail-t8-aliases.json').read_text(encoding='utf-8'))
    review = json.loads((root / 'research/retail-t8-handler-review.json').read_text(encoding='utf-8'))
    code = json.loads((root / 'research/retail-t8-code-labels.json').read_text(encoding='utf-8'))
    evidence_bytes = (root / review['evidence']['path']).read_bytes()
    if hashlib.sha256(evidence_bytes).hexdigest() != review['evidence']['sha256']:
        raise ValueError('Reviewed handler evidence hash changed')
    validate_handler_review(review, json.loads(evidence_bytes))
    result = {}
    for game in ('t7', 't8-retail'):
        for category, suffix in [('requirement', 'requirements'), ('extra_property', 'properties')]:
            content = handler_labels(labels(
                entries, game, category, legacy[category] if game == 't7' else None), aliases, game, category)
            if game == 't8-retail':
                content = reviewed_labels(content, review, suffix)
                content = code_labels(content, code, suffix)
            result[f'TKMovesets2/Resources/TKMInterface/editor_{game}_{suffix}.txt'] = content
    decisions = json.loads((root / 'research/retail-t8-mapping-decisions.json').read_text(encoding='utf-8'))['decisions']
    result['research/t7-to-retail-t8-mapping.json'] = json.dumps(annotate_handler_reviews(
        alias_evidence(resolve(mappings(entries), decisions), aliases), review), ensure_ascii=False, indent=2) + '\n'
    rules = json.loads((root / 'research/retail-t8-record-rules.json').read_text(encoding='utf-8'))['rules']
    header = ['// Generated by tools/generate_editor_reference_data.py; offline record conversion only.']
    for rule in rules:
        property_flag = 'true' if rule['category'] == 'extra_property' else 'false'
        header.append('    {%s, %du, %du, %du, %du},' % (property_flag, rule['t7_id'], rule['retail_id'], rule['param_min'], rule['param_max']))
    result['TKMovesets2/GameSpecific/MovesetConverters/RetailT8Rules.inc'] = '\n'.join(header) + '\n'
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for name, content in outputs().items():
        path = ROOT / name
        if args.check:
            if not path.exists() or path.read_text(encoding='utf-8') != content:
                raise SystemExit(f'Generated file is stale: {name}')
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding='utf-8')
    print('Reference labels and retail mapping candidates verified.' if args.check else 'Reference data generated.')


if __name__ == '__main__':
    main()
