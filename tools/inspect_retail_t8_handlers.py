"""Archive local handler evidence for asset candidates; never access a game process.

The research tracer is used only to decode archived trace PCs against executable
bytes. No emulator is started. Calls skipped in the historical traces remain a
limitation; these are selected paths, not exhaustive execution proofs.
"""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]


def identity(p):
    return dict(path=str(p.resolve()), bytes=p.stat().st_size,
                sha256=hashlib.sha256(p.read_bytes()).hexdigest())


def case_excerpt(text, identifier):
    matches = list(re.finditer(r'^(\s*)case\s+(0x[0-9a-fA-F]+|\d+)\s*:', text, re.M))
    for n, match in enumerate(matches):
        if int(match[2], 0) != identifier:
            continue
        indent = len(match[1].split('\n')[-1])
        start = match.start()
        for following in matches[n + 1:]:
            if len(following[1].split('\n')[-1]) != indent:
                continue
            body = text[match.end():following.start()]
            if not re.sub(r'\s|case\s+(?:0x[0-9a-fA-F]+|\d+)\s*:', '', body):
                continue
            return dict(line=text.count('\n', 0, start) + 1, text=text[start:following.start()].strip())
        return dict(line=text.count('\n', 0, start) + 1, text=text[start:start + 5000].strip())
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--research-root', type=Path, required=True)
    ap.add_argument('--t7-decompile', type=Path, required=True)
    ap.add_argument('--output', type=Path, default=ROOT / 'research/retail-t8-handler-evidence.json')
    a = ap.parse_args()
    sys.path.insert(0, str(a.research_root / 'scripts/analysis'))
    from movevm_trace import Tracer, BUILDS
    asset_path = ROOT / 'research/retail-t8-asset-comparison.json'
    aliases_path = ROOT / 'research/t7-to-retail-t8-aliases.json'
    assets = json.loads(asset_path.read_text(encoding='utf-8'))
    aliases = {(r['category'], r['t7_id']): r for r in json.loads(aliases_path.read_text())['entries']}
    selected = [r for r in assets['candidates'] if r['comparison_to_previous'] in ('disagrees', 'previously_unselected')
                or r['source_label'] and not r['target_label'] and r['status'] != 'competing_candidates']
    wanted = set()
    requests = []
    for r in selected:
        kind = 'prop' if r['source_id'] >= 0x8000 else 'req'
        category = 'extra_property' if r['category'] == 'properties' else 'requirement'
        old = aliases.get((category, r['source_id']))
        ids = dict(cnt=old['cnt_id'] if old else None, candidate=r['target_id'], previous=r['previous_target'])
        references = {}
        for role, identifier in ids.items():
            if identifier is None:
                continue
            build = 'cnt' if role == 'cnt' else 'retail'
            # A proposed cross-namespace requirement must be inspected in its own dispatcher.
            target_kind = 'prop' if identifier >= 0x8000 else 'req'
            key = (build, target_kind, identifier)
            wanted.add(key)
            references[role] = f'{build}:{target_kind}:0x{identifier:04X}'
        requests.append(dict(category=r['category'], source_id=r['source_id'], target_id=r['target_id'],
                             comparison=r['comparison_to_previous'], t7_dispatcher=kind, handlers=references))
    # Neighbouring banks distinguish a changed move's bank choice from a global ID renumbering.
    wanted.update({('retail', 'req', 0x167), ('retail', 'prop', 0x80C8)})
    cases = {}
    sources = [asset_path, aliases_path, Path(__file__)]
    for build in ('cnt', 'retail'):
        tracer = Tracer(BUILDS[build]['exe'])
        sources.append(BUILDS[build]['exe'])
        for kind in ('req', 'prop'):
            path = a.research_root / f'work/movevm/port/trace_{build}_{kind}.json'
            sources.append(path)
            data = json.loads(path.read_text())
            common = {}
            for flag in ('0', '1') if kind == 'prop' else ('0',):
                counts = Counter(pc for run in data['runs'].values() for pc in set(run[flag]['trace']))
                common[flag] = {pc for pc, n in counts.items() if n > len(data['runs']) * .2}
            for b, k, identifier in sorted(wanted):
                if (b, k) != (build, kind):
                    continue
                key = f'{build}:{kind}:0x{identifier:04X}'
                run = data['runs'].get(str(identifier))
                if run is None:
                    cases[key] = dict(missing_archived_trace=True)
                    continue
                paths = {}
                for flag, row in run.items():
                    trace = row['trace']
                    first = next((n for n, pc in enumerate(trace) if pc not in common[flag]), len(trace))
                    handler = list(dict.fromkeys(trace[first:]))[:1000]
                    instructions = []
                    for pc in handler:
                        ins = tracer.insn(pc)
                        instructions.append(dict(va=hex(pc), bytes=bytes(ins.bytes).hex(),
                                                 mnemonic=ins.mnemonic, operands=ins.op_str))
                    paths[flag] = dict(instructions=instructions, accesses=row['acc'], calls=[hex(c) if c>=0 else 'indirect' for c in row['calls']],
                        return_low_byte=row['ret'], error=row['err'], capped=row['capped'],
                        repeated_trace_pcs_collapsed=True, truncated_handler=len(set(trace[first:]))>1000)
                cases[key] = dict(paths=paths)
        if build == 'retail':
            # Include untaken conditional writes and shared helpers needed for manual review.
            windows = ((0x14189E5AD, 48), (0x1418A474C, 48), (0x1418A2F2B, 48),
                       (0x1418A3C91, 64), (0x14187B530, 0x270), (0x14187D404, 64),
                       (0x141880A98, 64), (0x1418A0583, 0x740))
            supplemental = {}
            for va, size in windows:
                offset = tracer.img.rva_to_off(va - tracer.img.base)
                supplemental[hex(va)] = [dict(va=hex(i.address), bytes=bytes(i.bytes).hex(),
                                               mnemonic=i.mnemonic, operands=i.op_str)
                                        for i in tracer.md.disasm(tracer.img.data[offset:offset+size], va)]
    t7_sources = {}
    t7_shared = {}
    for kind, filename in (('req', 'check_condition_v2.c'), ('prop', 'execute_property.c')):
        path = a.t7_decompile / filename
        sources.append(path)
        text = path.read_text(encoding='utf-8', errors='replace')
        labels = ('LABEL_1092',) if kind == 'prop' else ('LAB_1406f623b', 'LAB_1406f8f1d', 'LAB_1406f8fe7')
        for label in labels:
            found = re.search(r'^\s*' + label + r':', text, re.M)
            if found:
                t7_shared[label] = text[found.start():found.start()+1800].strip()
        for r in requests:
            if r['t7_dispatcher'] == kind:
                key = f't7:{kind}:0x{r["source_id"]:04X}'
                t7_sources[key] = case_excerpt(text, r['source_id'])
                r['handlers']['t7'] = key
        for identifier in ((0x161, 0x162) if kind == 'req' else (0x80A9, 0x80AC)):
            t7_sources[f't7:{kind}:0x{identifier:04X}'] = case_excerpt(text, identifier)
    bank_controls = dict(
        slots=[dict(slot=i, t7_read=0x160+i, t7_write=0x80A6+3*i,
                    retail_read=0x166+i, retail_write=0x80C5+3*i,
                    t7_offset=0x380+4*i, retail_offset=0x584+4*i) for i in range(3)],
        asset_support=[r for r in assets['candidates'] if r['category'] == 'requirements'
                       and r['source_id'] in (0x160, 0x80A6)])
    result = dict(schema_version=1, retail_build='26155785', archive_build=None, runtime_enabled=False,
        limitations='Archived synthetic paths with skipped calls and zeroed state. T7 decompile build unknown. Property-like requirements dispatch separately; no parameter transform implied.',
        sources=[identity(p) for p in sources], requests=requests, handlers=cases, t7_decompiles=t7_sources,
        t7_shared_blocks=t7_shared, supplemental_retail_disassembly=supplemental,
        stored_value_bank_controls=bank_controls)
    a.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'{len(requests)} reviews; {len(cases)} handlers; {len(t7_sources)} T7 cases -> {a.output}')


if __name__ == '__main__':
    main()
