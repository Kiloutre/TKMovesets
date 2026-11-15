"""Compare local T7 JSON extracts with retail disk assets; emit offline evidence only.

Uses the existing local research parser (recorded by hash), never a running game.
Names are resolved through the packaged retail name catalog, without fuzzy matching.
"""
import argparse
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]


def identity(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return dict(path=str(path.resolve()), bytes=path.stat().st_size, sha256=h.hexdigest())


def walk(rows, index, stop):
    if index is None or index == -1:
        return []
    if not 0 <= index < len(rows):
        raise ValueError(f'List index out of bounds: {index}/{len(rows)}')
    result = []
    for position in range(index, len(rows)):
        row = rows[position]
        result.append(row)
        if stop(row):
            return result
    raise ValueError('Unterminated record list')


def normalize(d, retail, names):
    """Keep all retail parameters and original list indices for traceable witnesses."""
    t = d
    moves = t['moves']
    hashes = [m['name'] if retail else names.get(m['name']) for m in moves]
    result = []
    for i, m in enumerate(moves):
        def reqs(index):
            rows = walk(t['requirements'], index,
                        lambda r: r['req'] == (1100 if retail else 881))
            return [(r['req'], tuple(r['params']) if retail else (r['param'] & 0xffffffff,))
                    for r in rows]

        cs = []
        for c in walk(t['cancels'], m['cancel' if retail else 'cancel_idx'],
                      lambda c: int(c['command'], 16) == 0x8000 if retail else c['command'] == 0x8000):
            cmd = int(c['command'], 16) if retail else c['command']
            if cmd == 0x8000:
                break
            target = c['move' if retail else 'move_id']
            # Alias IDs and special group/input commands need independent semantics.
            if target >= len(hashes) or target >= 0x8000 or cmd in (0x800D, 0x8012):
                continue
            if hashes[target] is None:
                continue
            ri = c['requirements' if retail else 'requirement_idx']
            cs.append(dict(key=(hashes[target], cmd), reqs=reqs(ri), index=ri))
        hits = []
        hi = m['hit_condition' if retail else 'hit_condition_idx']
        if hi is not None and hi >= 0:
            if hi >= len(t['hit_conditions']):
                raise ValueError('Hit condition index out of bounds')
            for position in range(hi, len(t['hit_conditions'])):
                h = t['hit_conditions'][position]
                ri = h['requirements' if retail else 'requirement_idx']
                r = reqs(ri)
                hits.append(dict(key=h['damage'], reqs=r, index=ri))
                if r and r[0][0] == (1100 if retail else 881):
                    break
            else:
                raise ValueError('Unterminated hit conditions')
        props = []
        table = t['extra_move_props' if retail else 'extra_move_properties']
        pi = m['extra_props' if retail else 'extra_properties_idx']
        for p in walk(table, pi, lambda p: p['id'] == 0):
            if p['id']:
                props.append((p['id'], tuple(p['params']) if retail else (p['value'] & 0xffffffff,),
                              p['frame' if retail else 'type']))
        result.append(dict(index=i, h=hashes[i], name=m['name'] if not retail else None,
                           anim=m['anim'] if retail else names.get(m['anim_name']),
                           cancels=cs, hits=hits, props=props))
    return result


def unique(rows, key):
    counts = Counter(r[key] for r in rows)
    return {r[key]: r for r in rows if r[key] is not None and counts[r[key]] == 1}


def compare(a, b, character, evidence, family=None):
    ua, ub = unique(a, 'h'), unique(b, 'h')
    shared = sorted(ua.keys() & ub.keys())
    same_anim = 0

    def add(category, x, y, witness):
        key = (category, x[0], y[0])
        e = evidence.setdefault(key, dict(contexts=set(), characters=set(), parameters=Counter(), witnesses=[]))
        # A shared requirement list reached from many moves is one independent context.
        e['contexts'].add((character, witness['kind'], witness['source_list'], witness['target_list'], witness['position']))
        e['characters'].add(family or character)
        e['parameters'][(x[1], y[1])] += 1
        if len(e['witnesses']) < 4:
            e['witnesses'].append(witness)

    for h in shared:
        x, y = ua[h], ub[h]
        anim_equal = x['anim'] is not None and x['anim'] == y['anim']
        same_anim += anim_equal
        base = dict(character=character, move=x['name'], source_move=x['index'], target_move=y['index'],
                    name_hash=f'0x{h:08X}', matching_animation_name=anim_equal,
                    source_animation_hash=None if x['anim'] is None else f"0x{x['anim']:08X}",
                    target_animation_hash=None if y['anim'] is None else f"0x{y['anim']:08X}")
        for kind in ('cancels', 'hits'):
            # Repeated target/command or damage keys are ambiguous, so skip them.
            ca, cb = unique(x[kind], 'key'), unique(y[kind], 'key')
            # Hit lists also require unchanged ordered damage sequence.
            if kind == 'hits' and [r['key'] for r in x[kind]] != [r['key'] for r in y[kind]]:
                continue
            for k in ca.keys() & cb.keys():
                ra, rb = ca[k]['reqs'], cb[k]['reqs']
                if not ra or len(ra) != len(rb) or ra[-1][0] != 881 or rb[-1][0] != 1100:
                    continue
                if any(p[1][0] != q[1][0] for p, q in zip(ra, rb)):
                    continue
                for pos, (p, q) in enumerate(zip(ra[:-1], rb[:-1])):
                    add('requirements', p, q, dict(base, kind=kind, source_list=ca[k]['index'],
                        target_list=cb[k]['index'], position=pos, source_record=ca[k]['index'] + pos,
                        target_record=cb[k]['index'] + pos,
                        repeated_first_parameter=sum(r[1][0] == p[1][0] for r in ra[:-1]) > 1))
        # Require the entire property sequence (frame and first parameter) to match.
        pa, pb = x['props'], y['props']
        if pa and len(pa) == len(pb) and all((p[1][0], p[2]) == (q[1][0], q[2]) for p, q in zip(pa, pb)):
            for pos, (p, q) in enumerate(zip(pa, pb)):
                add('properties', p, q, dict(base, kind='extra_properties', source_list=x['index'],
                    target_list=y['index'], position=pos, frame=p[2]))
    return dict(shared_unique_moves=len(shared), matching_animation_names=same_anim)


def animation_inventory(path, move_count):
    """Inspect disk header/list bounds and PANM identifiers, without decoding channels."""
    with path.open('rb') as f:
        header = f.read(0x98)
        if len(header) < 0x98:
            raise ValueError('Short ANMBIN header')
        size = path.stat().st_size
        categories = []
        for i, name in enumerate(('body', 'hands', 'face', 'swing', 'camera', 'extra')):
            count, = struct.unpack_from('<I', header, 4 + 4*i)
            refs, = struct.unpack_from('<I', header, 0x1c + 4*i)
            pool, = struct.unpack_from('<Q', header, 0x38 + 8*i)
            listing, = struct.unpack_from('<Q', header, 0x68 + 8*i)
            if pool + count * 0x38 > size or listing + refs * 4 > size:
                raise ValueError(f'ANMBIN {name} tables out of bounds')
            panms, absent = 0, 0
            keys = set()
            f.seek(pool)
            entries = f.read(count * 0x38)
            for j in range(count):
                key, offset = struct.unpack_from('<QQ', entries, j * 0x38)
                keys.add(key & 0xffffffff)
                if offset == 0:
                    absent += 1
                    continue
                if offset + 8 > size:
                    raise ValueError('ANMBIN animation pointer out of bounds')
                f.seek(offset + 4)
                if f.read(4) != b'PANM':
                    raise ValueError('Present ANMBIN entry lacks PANM identifier')
                panms += 1
            f.seek(listing)
            values = struct.unpack('<' + str(refs) + 'I', f.read(refs * 4))
            categories.append(dict(category=name, pool_entries=count, move_references=refs,
                references_equal_move_count=refs == move_count, panm_identifiers=panms,
                zero_data_offsets=absent,
                referenced_keys_absent_from_local_pool=len(set(values) - keys - {0})))
    return dict(identity(path), categories=categories)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--retail', type=Path, required=True)
    ap.add_argument('--t7', type=Path, required=True)
    ap.add_argument('--research-root', type=Path, required=True)
    ap.add_argument('--output', type=Path, default=ROOT / 'research/retail-t8-asset-comparison.json')
    args = ap.parse_args()
    formats = args.research_root / 'scripts/formats'
    sys.path.insert(0, str(formats))
    import motbin_retail_full as parser
    catalog_path = ROOT / 'TKMovesets2/Resources/TKMInterface/t8-retail_names.json'
    catalog = json.loads(catalog_path.read_text(encoding='utf-8'))
    by_name = defaultdict(set)
    for h, name in catalog['names'].items():
        by_name[name].add(int(h, 16))
    names = {n: next(iter(hs)) for n, hs in by_name.items() if len(hs) == 1}
    t7 = {}
    inputs = {}
    for path in sorted(args.t7.glob('t7_*/*.json')):
        d = json.loads(path.read_text(encoding='utf-8-sig'))
        t7[path.stem] = normalize(d, False, names)
        inputs[path.stem] = identity(path)
    # Character matching excludes common/system names shared by many T7 characters.
    frequency = Counter(h for ms in t7.values() for h in set(m['h'] for m in ms) if h is not None)
    distinctive = {c: {m['h'] for m in ms if m['h'] is not None and frequency[m['h']] <= 2}
                   for c, ms in t7.items()}
    evidence, characters, skipped = {}, [], []
    for path in sorted(args.retail.glob('*.motbin')):
        data = path.read_bytes()
        # Validate every table before invoking the research parser's record decoders.
        try:
            if len(data) < 0x318 or data[8:11] != b'TEK':
                raise ValueError('Unsupported retail motbin header')
            tabs = parser.R.tables(data)
            for table, (offset, count) in tabs.items():
                if offset > len(data) - 0x318 or count > (len(data) - 0x318 - offset) // parser.SIZES[table]:
                    raise ValueError(f'{table} exceeds file bounds')
            _, tables = parser.parse(data)
            ms = normalize(tables, True, names)
        except (ValueError, struct.error) as error:
            skipped.append(dict(code=path.stem, reason=str(error), retail=identity(path)))
            continue
        hs = {m['h'] for m in ms}
        ranking = sorted(((len(hs & ds), c) for c, ds in distinctive.items()), reverse=True)
        score, char = ranking[0]
        if score < 40 or score < 2 * ranking[1][0]:
            skipped.append(dict(code=path.stem, reason='insufficient distinctive same-character overlap', best=ranking[:2]))
            continue
        stats = compare(t7[char], ms, path.stem, evidence, family=char)
        animation = path.with_suffix('.anmbin')
        fighter = next((c for c in catalog['characters'] if c['code'] == path.stem), {})
        characters.append(dict(code=path.stem, t7_character=char, retail_name=fighter.get('name'),
            relationship_note='Raven compared with T7 Master Raven as a moveset lineage, not the identical fighter.' if path.stem == 'bbn' else None,
            matching_basis='unique name hashes; common names excluded',
            distinctive_matches=score, runner_up=ranking[1], t7=inputs[char], retail=identity(path),
            animations=animation_inventory(animation, len(ms)) if animation.exists() else None, **stats))
        print(f"{path.stem} / {char}: {stats}", flush=True)
    previous_path = ROOT / 'research/t7-to-retail-t8-aliases.json'
    previous = {('requirements' if r['category'] == 'requirement' else 'properties', r['t7_id']): r
                for r in json.loads(previous_path.read_text(encoding='utf-8'))['entries']}
    def labels(game, category):
        path = ROOT / f'TKMovesets2/Resources/TKMInterface/editor_{game}_{category}.txt'
        result = {}
        for line in path.read_text(encoding='utf-8-sig').splitlines():
            key, sep, value = line.partition(',')
            try:
                result[int(key, 0)] = value.strip()
            except ValueError:
                pass
        return result
    label_maps = {(game, category): labels(game, category)
                  for game in ('t7', 't8-retail') for category in ('requirements', 'properties')}
    rows = []
    targets = defaultdict(set)
    for category, source, target in evidence:
        targets[category, source].add(target)
    for (category, source, target), e in sorted(evidence.items()):
        old = previous.get((category, source))
        selected = old.get('review_retail_id') if old else None
        rows.append(dict(category=category, source_id=source, target_id=target,
            source_hex=f'0x{source:04X}', target_hex=f'0x{target:04X}',
            status='competing_candidates' if len(targets[category, source]) > 1 else
                   'cross_character_candidate' if len(e['characters']) >= 2 else 'single_character_candidate',
            independent_contexts=len(e['contexts']), characters=sorted(e['characters']),
            previous_status=old.get('effective_status', old['status']) if old else None,
            previous_target=selected,
            comparison_to_previous='new_source_id' if old is None else 'previously_unselected' if selected is None
                                   else 'agrees' if selected == target else 'disagrees',
            source_label=label_maps['t7', category].get(source),
            target_label=label_maps['t8-retail', category].get(target),
            parameter_samples=[dict(source=list(p), target=list(q), occurrences=n)
                               for (p, q), n in sorted(e['parameters'].items())],
            witnesses=e['witnesses'], runtime_enabled=False, parameter_transform=None))
    result = dict(schema_version=1, target_game='t8-retail', target_build=None,
        build_note='Archive identity is pinned by file hashes; parser profile references build 26155785, not independently verified for these assets.',
        policy='Offline candidates only; full first-parameter sequences and unique move names match within distinctive-name paired characters. Animation-name agreement is recorded separately. Extra retail parameters are observations, not a validated transform.',
        comparator=identity(Path(__file__)),
        parser_sources=[identity(formats / name) for name in ('motbin_retail_full.py', 'motbin_retail.py', 'tkdata.py')],
        name_catalog=identity(catalog_path), previous_alias_catalog=identity(previous_path), t7_collection=str(args.t7.resolve()),
        summary=dict(compared_assets=len(characters), t7_character_families=len({c['t7_character'] for c in characters}),
            skipped_assets=len(skipped), candidate_pairs=len(rows),
            requirements=len({r['source_id'] for r in rows if r['category'] == 'requirements'}),
            properties=len({r['source_id'] for r in rows if r['category'] == 'properties'}),
            statuses=dict(Counter(r['status'] for r in rows))),
        characters=characters, skipped=skipped, candidates=rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n', encoding='utf-8')
    print(json.dumps(result['summary']))


if __name__ == '__main__':
    main()
