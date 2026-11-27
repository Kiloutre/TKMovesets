"""Build a workspace for the T8 name tools from your own copy of the game.

    set T8_NAMES_WORKSPACE=<an empty folder of your choice>
    python bootstrap_workspace.py --tkdata "<game>/Polaris/Content/Binary/pak/tkdata.bin"
    python bootstrap_workspace.py --motbins <folder of already extracted *.motbin files>

Steps (each is skipped when its output exists; --force redoes them):

  1. game/motbin/mothead/bin/<code>.motbin     extracted from tkdata.bin (tkdata.py; needs zstandard and pycryptodome)
  2. game/names_dictionary.json                {decimal hash: name}: the names already known. Default source: the name
                                               table that ships with TKMovesets2; add public dictionaries with --names.
  3. game/movenames/<code>.csv, summary.csv    one row per move (motbin_retail.py), and ALL_movenames_retail.csv
  4. game/movesets/<code>.json                 every table of every move list (motbin_retail_full.py); the judge reads them
  5. recovery/method_lab/typed_corpus/         the typed corpus and the lists of unresolved hashes (build_name_corpora.py)

Nothing here needs the game to run and nothing is written outside the workspace.
"""
import argparse
import csv
import json
import os
import subprocess
import sys
from pathlib import Path

import t8names_paths as P

NAMES_JSON = P.SRC.parents[1]/'TKMovesets2'/'Resources'/'TKMInterface'/'t8-retail_names.json'


def run(*arguments):
    print('>', ' '.join(str(a) for a in arguments), flush=True)
    subprocess.run([sys.executable, *[str(a) for a in arguments]], check=True, cwd=P.SRC)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    source = ap.add_mutually_exclusive_group(required=True)
    source.add_argument('--tkdata', type=Path, help='tkdata.bin of the game')
    source.add_argument('--motbins', type=Path, help='folder that already holds <code>.motbin files')
    ap.add_argument('--names', type=Path, nargs='*', default=[], help='more dictionaries {decimal hash: name}, e.g. a public name_keys.json')
    ap.add_argument('--codes', help='comma-separated move-list codes; default: the codes of the TKMovesets2 name table')
    ap.add_argument('--no-shipped-names', action='store_true', help='do not start from the TKMovesets2 name table')
    ap.add_argument('--force', action='store_true')
    a = ap.parse_args()
    game = P.WORKSPACE/'game'
    game.mkdir(parents=True, exist_ok=True)
    shipped = json.loads(NAMES_JSON.read_text(encoding='utf-8')) if NAMES_JSON.exists() else dict(characters=[], names={})

    # 2. dictionary (written first: the extraction of step 1 does not need it, the tables of step 3 do)
    dictionary = game/'names_dictionary.json'
    if a.force or not dictionary.exists():
        from kamuiHash import getKamuiHashDigest as digest
        names = {} if a.no_shipped_names else {str(int(k, 16)): v for k, v in shipped['names'].items()}
        for path in a.names:
            names.update({str(int(k)): v for k, v in json.loads(path.read_text(encoding='utf-8')).items()})
        # placeholders of unknown animations (ak<HASH>___) are not names: they do not re-hash
        dropped = [k for k, v in names.items() if digest(v) != int(k)]
        for k in dropped:
            del names[k]
        if dropped:
            print('dictionary: %d entries left out because they do not re-hash (placeholders)' % len(dropped))
        dictionary.write_text(json.dumps(names, ensure_ascii=False), encoding='utf-8')
        print('dictionary: %d names' % len(names))

    # 1. motbins
    if a.motbins:
        motbins = a.motbins
    else:
        motbins = game/'motbin'/'mothead'/'bin'
        if a.force or not any(motbins.glob('*.motbin')):
            codes = a.codes or ','.join(c['code'] for c in shipped['characters'])
            if not codes:
                raise SystemExit('No move-list codes: pass --codes aml,ant,... (three-letter codes of mothead/bin/<code>.motbin).')
            run('tkdata.py', a.tkdata, '--extract', game/'motbin', '--codes', codes, '--match', r'^mothead/bin/.*\.motbin$')
    files = sorted(motbins.glob('*.motbin'))
    if not files:
        raise SystemExit('No .motbin file in %s' % motbins)

    # 3. move-name tables
    if a.force or not (P.MOVENAMES/'ALL_movenames_retail.csv').exists():
        run('motbin_retail.py', motbins, '--names', dictionary, '--out', P.MOVENAMES)
        rows = 0
        with (P.MOVENAMES/'ALL_movenames_retail.csv').open('w', encoding='utf-8', newline='') as out:
            writer = None
            for path in sorted(P.MOVENAMES.glob('*.csv')):
                if len(path.stem) != 3 and path.stem != 'test':
                    continue
                with path.open(encoding='utf-8', newline='') as handle:
                    for r in csv.DictReader(handle):
                        if writer is None:
                            writer = csv.DictWriter(out, fieldnames=['code'] + list(r), lineterminator='\n'); writer.writeheader()
                        writer.writerow(dict(r, code=path.stem)); rows += 1
        print('ALL_movenames_retail.csv: %d rows' % rows)

    # 4. full move tables
    if a.force or not any(P.MOVESETS.glob('*.json')):
        run('motbin_retail_full.py', motbins, '--names', dictionary, '--out', P.MOVESETS)

    # 5. typed corpus
    P.LAB.mkdir(parents=True, exist_ok=True)
    if a.force or not (P.LAB/'typed_corpus'/'rows.jsonl.gz').exists():
        if not P.LEGACY.exists():
            print('note: no older-game extracts in %s (T8_NAMES_LEGACY): the corpus holds T8 only' % P.LEGACY)
        run('build_name_corpora.py')
    print('workspace ready:', P.WORKSPACE)


if __name__ == '__main__':
    main()
