"""Vendor offline alias evidence without enabling converter rules or game access."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = 'research/t7-to-retail-t8-aliases.json'


def load_handoff(directory):
    names = ['aliases.json', 'provenance.json', 'handler-resolution-835f.json',
             'structural-markers.json', 'investigation/handler-evidence-835f.json',
             'investigation/vote-witnesses-835f.json']
    data, hashes = {}, {}
    for name in names:
        raw = (directory / name).read_bytes()
        data[name] = json.loads(raw.decode('utf-8-sig'))
        hashes[name] = hashlib.sha256(raw).hexdigest()
    aliases = data['aliases.json']
    if aliases['target']['game'] != 'Tekken8 retail' or aliases['target']['steam_build_id'] != '26155785':
        raise ValueError('Expected retail build 26155785; CNT is not a retail target')
    if aliases['runtime_enabled'] or aliases['loadable_moveset']:
        raise ValueError('Handoff must be review-only')
    seen = set()
    for row in aliases['records']:
        key = row['category'], row['t7_id']
        if key in seen or row['runtime_enabled'] or row['parameter_transform'] is not None:
            raise ValueError('Duplicate alias or unsupported runtime/parameter authorization')
        seen.add(key)
    return {
        'schema_version': 1, 'target_game': 't8-retail', 'target_build': '26155785',
        'source_build': aliases['source'], 'intermediate_build': aliases['intermediate'],
        'target_identity': aliases['target'], 'runtime_enabled': False,
        'policy': 'Offline ID evidence only; preserve historical status, effective status, conflicts and unknown transforms. No converter rules generated.',
        'source_handoff': {'path': str(directory), 'file_sha256': hashes,
                           'snapshot_refs_relative_to': 'source_handoff.path',
                           'provenance': data['provenance.json']},
        'handler_resolution': data['handler-resolution-835f.json'],
        'handler_evidence': data['investigation/handler-evidence-835f.json'],
        'vote_witnesses': data['investigation/vote-witnesses-835f.json'],
        'structural_markers': data['structural-markers.json']['markers'],
        'entries': aliases['records'],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--handoff', required=True, type=Path)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    content = json.dumps(load_handoff(args.handoff), ensure_ascii=False, indent=2) + '\n'
    output = ROOT / OUTPUT
    if args.check:
        if output.read_text(encoding='utf-8') != content:
            raise SystemExit('Vendored alias evidence is stale')
    else:
        output.write_text(content, encoding='utf-8')
    print('Retail alias evidence verified.' if args.check else 'Retail alias evidence imported; runtime rules unchanged.')


if __name__ == '__main__':
    main()
