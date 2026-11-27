"""Tables of the keyed searches, generated from the result files (no number is typed by hand).

Reads key_confirmed_names.csv (stored lists), keyed_tiny.json (lengths 1 to 4), keyed_short_results.json and
keyed_short_names.csv (lengths 5 to 10), keyed_long_results.json and keyed_long_names.csv (lengths 11 to 20), the
same two files of every later round (keyed_long_results_round<k>.json, keyed_long_names_round<k>.csv) and keyed_expand.json.
Writes keyed_search_tables.md and keyed_confirmed_all.csv (one row per key-confirmed name of an open hash or of a
doubtful label, every source merged). Every name is re-hashed and its keys are counted again here.

  python keyed_report_tables.py
"""
import collections as co
import csv
import json

from name_judge_data import HERE, load_all_overlays, load_t8_rows, load_targets
import keyed_short_run as KS
import move_key as K
from recover import digest

NL = chr(10)


def rows_of(name):
    path = HERE/name
    if not path.exists():
        return []
    with path.open(encoding='utf-8', newline='') as f:
        return list(csv.DictReader(f))


def load_json(name):
    path = HERE/name
    return json.loads(path.read_text(encoding='utf-8')) if path.exists() else None


def main():
    keys = K.load_keys(); overlay = {int(h, 16) for h in load_all_overlays()}
    open_hashes = {int(t['hash'], 16) for t in load_targets('move')}-overlay
    doubtful = {digest(name): name for name in KS.DOUBTFUL}
    out = []; add = out.append
    merged = {}

    def note(value, name, source, reading='', chance=''):
        ok, total = K.agree(keys, value, name)
        assert digest(name) == value and ok >= 1, (value, name)
        e = merged.setdefault((value, name), dict(hash='0x%08X' % value, name=name, length=len(name), keys_agreeing=ok, keys_of_the_hash=total, sources=[], reading='', chance=''))
        e['sources'].append(source)
        if reading:
            e['reading'] = reading; e['chance'] = chance

    for r in rows_of('key_confirmed_names.csv'):
        if r['state'] == 'open hash':
            note(int(r['hash'], 16), r['name'], 'stored lists: '+r['lists'].split(' | ')[0].replace('judge round', 'round'))
    for r in rows_of('keyed_short_names.csv'):
        note(int(r['hash'], 16), r['name'], 'complete search of length %s' % r['length'], 'model cost %.0f bits' % float(r['character_model_cost_bits']))
    for path in sorted(HERE.glob('keyed_long_names*.csv')):
        again = ' (round %s)' % path.stem.split('round')[1] if 'round' in path.stem else ''
        for r in rows_of(path.name):
            if (int(r['hash'], 16), r['name']) in merged and again:
                continue                    # already known from the first round: keep its reading
            note(int(r['hash'], 16), r['name'], 'keyed search of length %s%s' % (r['length'], again), r['reading'], r['chance_names_expected_with_a_tail_this_cheap'])
    expand = load_json('keyed_expand.json')
    for r in (expand or {}).get('names_confirmed_by_the_key', []):
        note(int(r['hash'], 16), r['name'], 'small edit of `%s`' % r['from_name'], 'small edit of a confirmed name')

    short = load_json('keyed_short_results.json'); tiny = load_json('keyed_tiny.json'); long_ = load_json('keyed_long_results.json')
    # cost of the control agreements of the complete searches: what a chance name costs there
    floor = None; floors = {}
    if short:
        costs = [c['character_model_cost_bits'] for e in short['by_length'].values() for c in e['control_agreements']]
        floor = min(costs) if costs else None
        floors = {int(n): e['cheapest_control_agreement_bits'] for n, e in short['by_length'].items() if e['cheapest_control_agreement_bits'] is not None}

    def verdict(e):
        """real / uncertain / chance, from the keys and the reading of the run that produced the name."""
        if e['keys_agreeing'] != e['keys_of_the_hash']:
            return 'chance'
        if e['keys_of_the_hash'] >= 2:
            return 'real'
        if any(s.startswith('stored lists') for s in e['sources']):
            return 'real'                       # the stored lists hold too few names for a chance agreement (0 in 4.6 million decoy names)
        if e['reading'].startswith('model cost'):
            # complete search: a chance name is a random string; the cheapest control agreement of that length is the floor of chance
            cost = float(e['reading'].split()[2]); limit = floors.get(e['length'], floor)
            return 'real' if limit is None or cost <= 0.75*limit else ('uncertain' if cost < limit else 'chance')
        if e['reading'].startswith('small edit'):
            return 'real'                       # the edits of all names together are some 30 million strings: no chance agreement with a key
        if 'cheaper than chance' in e['reading'] or 'shares a part with the animation' in e['reading'] or 'starts like a named neighbour' in e['reading']:
            return 'real'
        return 'chance' if 'not distinguishable' in e['reading'] else 'uncertain'

    # Row context, as a second opinion that uses nothing of the search: does the name start like a named move within three rows (text up to the
    # first underscore, at least three characters with it), and does it share a part of three characters or more with the animation name of its row?
    accepted_names = load_all_overlays(); by_row = {}
    for r in load_t8_rows():
        by_row.setdefault(r['code'], {})[int(r['id'])] = r

    def context(name, value):
        parts = [t.lower() for t in name.split('_') if t]; head = name.split('_')[0]+'_' if '_' in name else ''; shared = set(); like = False
        for code, i, _ in keys.get(value, []):
            r = by_row.get(code, {}).get(i)
            if r and r['anim_status'] == 'verified' and r['anim_name']:
                anim = [t.lower() for t in r['anim_name'].split('_') if t]; shared |= {t for t in parts if len(t) >= 3 and t in anim}
            for j in range(i-3, i+4):
                q = by_row.get(code, {}).get(j)
                if q and j != i:
                    known = q['move_name'] if q['move_status'] == 'verified' else accepted_names.get(q['move_hash'], '')
                    like = like or bool(known and len(head) >= 3 and known.startswith(head))
        return sorted(shared), like

    chance_context = co.Counter(); true_context = co.Counter()
    sample = json.loads((HERE/'keyed_long_targets.json').read_text(encoding='utf-8')) if (HERE/'keyed_long_targets.json').exists() else dict(targets=[])
    answers = {t['hash']: t['name'] for t in sample['targets'] if t['group'] == 'known answer'}
    answers.update({t['hash']: t['name'] for t in (load_json('keyed_xl_targets.json') or dict(targets=[]))['targets'] if t['group'] == 'known answer'})
    folders = sorted((HERE/'keyed_runs').glob('long*'))+[HERE/'keyed_runs'/'xl']                    # every round, and lengths 21 to 24
    run_files = [p for folder in folders if folder.is_dir() for p in sorted(folder.glob('n[0-9][0-9].json'))]
    for path in run_files:
        for h in json.loads(path.read_text(encoding='utf-8'))['hits']:
            shared, like = context(h['name'], int(h['hash'], 16))
            if h['kind'] != 'key':
                chance_context['names'] += 1; chance_context['animation part'] += bool(shared); chance_context['neighbour start'] += like
            elif answers.get(h['hash']) == h['name']:
                true_context['names'] += 1; true_context['animation part'] += bool(shared); true_context['neighbour start'] += like

    table = []
    for (value, name), e in merged.items():
        group = 'doubtful label' if value in doubtful else 'open'
        places = keys[value]; shared, like = context(name, value); v = verdict(e)
        e['context'] = '; '.join(x for x in ('starts like a named neighbour' if like else '', ('shares %s with the animation name' % ', '.join('`%s`' % s for s in shared)) if shared else '') if x)
        # names recorded since by the coordinating session's acceptance tool (accept_move_key_confirmed.py) are marked; this file never accepts anything
        now = accepted_names.get('0x%08X' % value)
        assert now in (None, name) or v != 'real', 'a name taken as real conflicts with the accepted name %s: %s' % (now, name)
        status = 'accepted since (overlay of the coordinating session)' if now == name else 'key-confirmed candidate, not accepted'
        table.append(dict(e, group=group, label=doubtful.get(value, ''), verdict=v, rows=' '.join('%s:%d' % (c, i) for c, i, _ in places[:3])+(' ...' if len(places) > 3 else ''),
                          sources=' | '.join(sorted(set(e['sources']))), status=status))
    table.sort(key=lambda r: (r['group'], {'real': 0, 'uncertain': 1, 'chance': 2}[r['verdict']], r['length'], r['name']))
    fields = ['hash', 'name', 'length', 'keys_agreeing', 'keys_of_the_hash', 'verdict', 'group', 'label', 'reading', 'chance', 'context', 'rows', 'sources', 'status']
    with (HERE/'keyed_confirmed_all.csv').open('w', encoding='utf-8', newline='') as f:
        writer = csv.DictWriter(f, fields); writer.writeheader(); writer.writerows(table)

    add('# Keyed searches: tables'); add('')
    add('Generated by `keyed_report_tables.py` from the result files. This file accepts nothing: the names are key-confirmed candidates. The column State shows which of them the coordinating session has recorded since.'); add('')
    real_open = [r for r in table if r['group'] == 'open' and r['verdict'] == 'real']
    hashes_real = {r['hash'] for r in real_open}
    started = sum(1 for t in sample['targets'] if t['group'] == 'open'); taken = {r['hash'] for r in real_open if r['status'].startswith('accepted')}
    add('Open move hashes when the keyed searches started: %d. Hashes among them with a key-confirmed name taken as real: **%d**, of which %d are accepted since by the coordinating session (standing owner rule, `accept_move_key_confirmed.py`) and %d are candidates. Open move hashes now: %d. Doubtful labels with a true name found: **%d of %d**.' % (
        started, len(hashes_real), len(taken), len(hashes_real)-len(taken), len(open_hashes), len({r['hash'] for r in table if r['group'] == 'doubtful label' and r['verdict'] == 'real'}), len(doubtful))); add('')
    if chance_context['names']:
        add('Row context as a second opinion (it uses nothing of the search). Of %d control agreements (chance names) of the keyed searches of every round, %d start like a named move within three rows and %d share a part with the animation name of their row. Of %d hidden known answers returned (a name returned by two rounds counts twice), %d and %d do.' % (
            chance_context['names'], chance_context['neighbour start'], chance_context['animation part'], true_context['names'], true_context['neighbour start'], true_context['animation part'])); add('')

    add('## 1. Coverage and known answers, by length'); add('')
    add('| Length | What was covered | Hidden known answers returned | Other names returned for their hashes | Control agreements (chance names) | Time |')
    add('|---|---|---|---|---|---|')
    if tiny:
        add('| 1 to 4 | every string (%s) | none of these lengths in the sample | | | under 2 minutes |' % format(tiny['strings'], ','))
    if short:
        ka = co.Counter(len(t['name']) for t in KS.load() if t['group'] == 'known answer')
        found = co.Counter()
        truth = {t['hash']: t['name'] for t in KS.load() if t['group'] == 'known answer'}
        for n, e in sorted(short['by_length'].items(), key=lambda kv: int(kv[0])):
            controls = sum(e['confirmed_with_a_masked_key'])+e['confirmed_with_another_code']+e['decoy_names_confirmed_by_the_key']
            hits = [h for h in json.loads((HERE/'keyed_runs'/'short'/('n%s.json' % n)).read_text(encoding='utf-8'))['hits']['real'] if h['kind'] == 'key' and h['hash'] in truth]
            right = sum(1 for h in hits if truth[h['hash']] == h['name'])
            add('| %s | every string (%.3g); %.3g names with a wanted hash | %d of %d | %d | %d; %.2f expected per comparison | %.0f s on 10 workers |' % (
                n, e['strings_of_this_length'], e['names_with_a_wanted_hash'], right, ka[int(n)], len(hits)-right, controls, e['chance_agreements_expected_per_comparison'], e['wall_seconds']))
    if long_:
        for n, e in sorted(long_['by_length'].items(), key=lambda kv: int(kv[0])):
            add('| %s | first %d characters within %g bits: %.3g bodies; last %d characters free | %d of %d within the budget (%d known answers) | %d | %d | %.0f s on 10 workers |' % (
                n, e['body_length'], e['budget_bits'], e['bodies'], int(n)-e['body_length'], e['known_answers_returned'], e['known_answers_within_the_budget'], e['known_answers'],
                e['other_names_returned_for_known_answer_hashes'], e['hash_mask_controls']+e['key_mask_controls']+e['wrong_code_controls'], e['wall_seconds']))
    # later rounds: the names taken as real so far are added to the character model and the budgets are wider
    for path in sorted(HERE.glob('keyed_long_results_round*.json')):
        number = path.stem.split('round')[1]; later = json.loads(path.read_text(encoding='utf-8'))
        for n, e in sorted(later['by_length'].items(), key=lambda kv: int(kv[0])):
            add('| %s, round %s | first %d characters within %g bits of the model with the confirmed names: %.3g bodies; last %d characters free | %d of %d within the budget (%d known answers) | %d | %d | %.0f s on 10 workers |' % (
                n, number, e['body_length'], e['budget_bits'], e['bodies'], int(n)-e['body_length'], e['known_answers_returned'], e['known_answers_within_the_budget'], e['known_answers'],
                e['other_names_returned_for_known_answer_hashes'], e['hash_mask_controls']+e['key_mask_controls']+e['wrong_code_controls'], e['wall_seconds']))
    # lengths 21 to 24 (keyed_xl_run.py): tail from the name hash for every wanted hash, the key compared in full; its own hidden known answers
    xl = load_json('keyed_long_results_xl.json')
    for n, e in sorted((xl or dict(by_length={}))['by_length'].items(), key=lambda kv: int(kv[0])):
        add('| %s | first %d characters within %g bits of the model with the confirmed names: %.3g bodies; last %d characters free (from the name hash, per wanted hash) | %d of %d within the budget (%d known answers) | %d | %d | %.0f s on 10 workers |' % (
            n, e['body_length'], e['budget_bits'], e['bodies'], int(n)-e['body_length'], e['known_answers_returned'], e['known_answers_within_the_budget'], e['known_answers'],
            e['other_names_returned_for_known_answer_hashes'], e['hash_mask_controls']+e['key_mask_controls']+e['wrong_code_controls'], e['wall_seconds']))
    add('')

    def names_table(rows, with_label=False):
        add('| Hash | Name | Length | Keys | %sState | Where it came from | Reading | Row context | Rows |' % ('Label it replaces | ' if with_label else ''))
        add('|---|---|---|---|%s---|---|---|---|---|' % ('---|' if with_label else ''))
        for r in rows:
            add('| `%s` | `%s` | %d | %d of %d | %s%s | %s | %s%s | %s | %s |' % (r['hash'], r['name'], r['length'], r['keys_agreeing'], r['keys_of_the_hash'], ('`%s` | ' % r['label']) if with_label else '',
                                                                          'accepted' if r['status'].startswith('accepted') else 'candidate',
                                                                          r['sources'], r['reading'] or 'in a stored list', (' (%s chance names expected)' % r['chance']) if r['chance'] else '',
                                                                          r['context'] or '-', r['rows']))
        add('')

    add('## 2. Hashes open at the start: key-confirmed names taken as real (%d names, %d hashes)' % (len(real_open), len(hashes_real))); add('')
    names_table(real_open)
    add('## 3. Doubtful labels: the names the key confirms'); add('')
    names_table([r for r in table if r['group'] == 'doubtful label' and r['verdict'] == 'real'], with_label=True)
    rest = [r for r in table if r['verdict'] != 'real']
    add('## 4. One-key agreements that are uncertain or look like chance names (%d)' % len(rest)); add('')
    add('| Hash | Name | Length | Keys | Verdict | Where it came from | Reading | Row context |'); add('|---|---|---|---|---|---|---|---|')
    for r in rest:
        add('| `%s` | `%s` | %d | %d of %d | %s | %s | %s%s | %s |' % (r['hash'], r['name'], r['length'], r['keys_agreeing'], r['keys_of_the_hash'], r['verdict'], r['sources'], r['reading'],
                                                                  (' (%s chance names expected)' % r['chance']) if r['chance'] else '', r['context'] or '-'))
    add('')
    (HERE/'keyed_search_tables.md').write_text(NL.join(out)+NL, encoding='utf-8')
    print('names', len(table), dict(co.Counter((r['group'], r['verdict']) for r in table)), 'open hashes with a real name', len(hashes_real))
    for r in table:
        print(r['group'][:8], r['verdict'].ljust(9), r['hash'], r['name'].ljust(22), '%d/%d' % (r['keys_agreeing'], r['keys_of_the_hash']), '|', r['sources'][:44], '|', r['reading'][:50], '|', r['context'][:60], r['label'])


if __name__ == '__main__':
    main()
