"""Build research/retail-t8-code-labels.json from the retail handler review CSVs (Dynamic game analysis).

The CSVs name every retail requirement / extra property from its decompiled handler (Steam build 26155785).
Only names with high or medium confidence are kept. tools/generate_editor_reference_data.py uses them to fill ids
that have no editor label; it never replaces an existing label.

    python tools/import_retail_code_labels.py REQUIREMENTS_CSV EXTRAPROPS_CSV
"""
import argparse
import csv
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / 'research/retail-t8-code-labels.json'
OPS = [('GE', '>='), ('LE', '<='), ('GT', '>'), ('LT', '<'), ('Eq', '=='), ('Ne', '!=')]
# Readable wording for the approved replacements (labels the code contradicts), from each handler's meaning.
CURATED = {
    ('requirements', 150): 'Player on left side and backturned',
    ('requirements', 151): 'Player on right side and backturned',
    ('requirements', 508): '"Related Player" Current Move != param',
    ('requirements', 784): 'Wave/enemy index <= X',
    ('requirements', 785): 'Wave/enemy index == X (0-based)',
    ('requirements', 796): 'Not in story battle X',
    ('properties', 0x80A6): 'Subtract value from u16 P+0x181C',
    ('properties', 0x80A7): 'Set u16 P+0x181E to value',
    ('properties', 0x80A8): 'Add value to u16 P+0x181E',
    ('properties', 0x827C): 'Set projectile spawn bone',
}
DEAD = re.compile(r'never (be )?true|always (returns )?false|does nothing|no-op|empty slot|dead in retail')


def worded(name):
    """'OpponentDistanceLE' -> 'Opponent Distance <='."""
    for suffix, op in OPS:
        if name.endswith(suffix) and len(name) > len(suffix):
            return worded(name[:-len(suffix)]) + ' ' + op
    return re.sub(r'(?<=[a-z])(?=[A-Z])', ' ', name).replace('_', ' ').strip()


def rows(path, category, current):
    result, replacements = [], []
    for row in csv.DictReader(open(path, encoding='utf-8')):
        if not row['name'] or row['confidence'] not in ('high', 'medium'):
            continue
        label = worded(row['name'])
        if row['label_verdict'] == 'contradicted':
            # Approved by the owner on 2026-11-14: the code contradicts the editor label, so the code name replaces it.
            previous = current.get(int(row['id']))
            if previous is not None:
                replacements.append({'category': category, 'id': int(row['id']),
                                     'label': CURATED.get((category, int(row['id'])), label), 'previous': previous,
                                     'confidence': row['confidence'], 'meaning': row['meaning'], 'evidence': row['evidence']})
            continue
        if DEAD.search((row['meaning'] + ' ' + row['notes']).lower()) or row['name'].startswith(('NoOp', 'Noop', 'AlwaysFalse')):
            label += ' [no effect in retail]'
        result.append({'category': category, 'id': int(row['id']), 'label': label, 'name': row['name'],
                       'confidence': row['confidence'], 'meaning': row['meaning'], 'evidence': row['evidence']})
    return result, replacements


def current_labels(name):
    """The editor labels as the generator builds them from the reference sheets, before any code label."""
    import importlib.util
    spec = importlib.util.spec_from_file_location('generator', ROOT / 'tools/generate_editor_reference_data.py')
    generator = importlib.util.module_from_spec(spec); spec.loader.exec_module(generator)
    generator.code_labels = lambda text, catalog, category: text
    text = generator.outputs()['TKMovesets2/Resources/TKMInterface/' + name]
    return {int(line.partition(', ')[0]): line.partition(', ')[2] for line in text.splitlines() if line}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('requirements')
    parser.add_argument('extraprops')
    args = parser.parse_args()
    requirements, replaced_requirements = rows(args.requirements, 'requirements', current_labels('editor_t8-retail_requirements.txt'))
    properties, replaced_properties = rows(args.extraprops, 'properties', current_labels('editor_t8-retail_properties.txt'))
    labels, replacements = requirements + properties, replaced_requirements + replaced_properties
    catalog = {
        'schema_version': 1, 'target_game': 't8-retail', 'target_build': '26155785', 'mode': 'fill_missing',
        'runtime_enabled': False,
        'policy': 'Names derived from the decompiled retail handlers (TK__Requirement::IsTrue 0x14187B7A0, '
                  'ExecuteExtraproperty 0x141898CA0). Fill ids without an editor label; replace a label only through an approved '
                  '`replacements` entry (owner approval 2026-11-14 for the labels the code contradicts).',
        'sources': [{'file': Path(p).name, 'sha256': hashlib.sha256(Path(p).read_bytes()).hexdigest()}
                    for p in (args.requirements, args.extraprops)],
        'labels': labels,
        # Explicit replacements of labels the code contradicts; each applies only while the label is still `previous`.
        'replacements': replacements,
    }
    OUTPUT.write_text(json.dumps(catalog, ensure_ascii=False, indent=1) + '\n', encoding='utf-8')
    print(f'{len(labels)} labels, {len(replacements)} replacements written to {OUTPUT.relative_to(ROOT)}')


if __name__ == '__main__':
    main()
