"""Inventory separate local move/animation corpora; no training or searches."""
import t8names_paths as _P
import collections as co
import csv
import hashlib
import json
from pathlib import Path
import re
from tail_solver import HERE
from recover import read,digest

ROOT = None  # locations come from t8names_paths
T7=_P.LEGACY
PLACEHOLDER=re.compile(r'^(?:nk|ak)[0-9a-fA-F]{8}_*$')


def valid(name):return isinstance(name,str) and bool(name) and not PLACEHOLDER.fullmatch(name)


def main():
    sets={key:set() for key in ('t7_moves','t7_animations','t8_moves','t8_animations')}
    files=[];t7rows=0
    for path in sorted(T7.glob('t7_*/*.json')):
        raw=path.read_bytes();d=json.loads(raw);moves=d.get('moves',[]);t7rows+=len(moves)
        for row in moves:
            for column,key in [('name','t7_moves'),('anim_name','t7_animations')]:
                if valid(row.get(column)):sets[key].add(row[column])
        # These are review flags, not assertions of official/custom provenance.
        flags=[]
        if d.get('original_hash')!=d.get('last_calculated_hash'):flags.append('stored_hashes_differ')
        if re.search(r'mod|old|TAG2|CHUN_LI|MR\.X|GILL|VAMPIRE|NEGANE?V?2|FRV_n',path.stem,re.I):flags.append('label_needs_variant_review')
        files.append(dict(path=str(path),sha256=hashlib.sha256(raw).hexdigest(),rows=len(moves),
            version=d.get('version'),character_name=d.get('character_name'),tekken_character_name=d.get('tekken_character_name'),
            extraction_date=d.get('extraction_date'),flags=flags))
    path=_P.MOVENAMES/'ALL_movenames_retail.csv';raw=path.read_bytes()
    with path.open(encoding='utf-8-sig',newline='') as f:t8rows=list(csv.DictReader(f))
    unknown=co.Counter();unknown_hashes=co.defaultdict(set)
    for row in t8rows:
        for column,key,hashcol in [('name','t8_moves','name_hash'),('anim_name','t8_animations','anim_hash')]:
            if valid(row.get(column)):sets[key].add(row[column])
            else:unknown[key]+=1;unknown_hashes[key].add(row[hashcol])
    known=read('known_anim_names.csv');mapping=read('known_move_to_anim.csv')
    used_anim={r['anim_name'] for r in known};used_move={r['move_name'] for r in mapping if valid(r['move_name'])}
    unused_t7_anim=sets['t7_animations']-used_anim;unused_t7_move=sets['t7_moves']-used_move
    extra_labels=[]
    for name in sorted(sets['t8_animations']-used_anim):
        hashes=sorted({r['anim_hash'] for r in t8rows if r['anim_name']==name})
        extra_labels.append(dict(name=name,source_hashes=hashes,recomputed_hash=f'0x{digest(name):08X}',
            matches_all_source_hashes=all(digest(name)==int(h,16) for h in hashes),
            rejected_by_original_broad_marker_filter=name.startswith(('nk','ak'))))
    result=dict(scope='Local extracted-name inventory only. No model fit, hash recovery or master changes.',
        prior_training=dict(animation_names=len(used_anim),move_mapping_rows=len(mapping),distinct_nonempty_mapped_move_names=len(used_move),
            source_files=['known_anim_names.csv','known_move_to_anim.csv'],
            independent_move_name_model=False,independent_t7_corpus_import=False,
            treatment='Animation names train grammar; move names are separate conditional inputs for mappings/rules/category/initial priors. Unknown targets are animation hashes only.'),
        local_corpora={key:dict(unique_nonempty_nonplaceholder_names=len(values)) for key,values in sets.items()},
        t7=dict(extract_files=len(files),move_rows=t7rows,metadata_versions=dict(co.Counter(f['version'] for f in files)),
            files_flagged_for_review=sum(bool(f['flags']) for f in files),
            review_caveat='t7_ filenames and Tekken7 version fields do not prove unmodified official content. Flags are heuristics, not classifications.',files=files),
        t8=dict(source=str(path),sha256=hashlib.sha256(raw).hexdigest(),rows=len(t8rows),
            codes=len({r['code'] for r in t8rows}),unresolved_or_empty_rows=dict(unknown),
            unresolved_or_empty_unique_hashes={k:len(v) for k,v in unknown_hashes.items()},
            valid_animation_labels_missing_from_handoff=extra_labels),
        overlap=dict(t7_animation_names_already_in_training=len(sets['t7_animations']&used_anim),
            t7_animation_names_absent_from_training=len(unused_t7_anim),
            t7_move_names_already_in_mapped_inputs=len(sets['t7_moves']&used_move),
            t7_move_names_absent_from_mapped_inputs=len(unused_t7_move),
            t8_anim_names_absent_from_training=len(sets['t8_animations']-used_anim),
            training_anim_names_absent_from_full_t8_export=len(used_anim-sets['t8_animations']),
            t8_move_names_absent_from_mapped_inputs=len(sets['t8_moves']-used_move)),
        caveats=['Counts concern the located exports, not a claim of all names ever used in either game.',
            'Raw T7 novel-name counts include unreviewed variants/custom extracts and are not clean extra training counts.',
            'Some T7 names are already present indirectly through their use in T8.',
            'Future evaluation must prevent cross-game copies of held-out animation labels from leaking into training.'])
    (HERE/'name_corpus_audit.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='t7'},indent=2))
    print('T7 extracts',len(files),'rows',t7rows,'review flags',sum(bool(f['flags']) for f in files))


if __name__=='__main__':main()
