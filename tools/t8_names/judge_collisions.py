"""Run the name judge on every move hash of the conflict file that has two or more re-hashing names.

Every name involved in any collision is hidden from the judge's knowledge base for every case (class Exclusion), so
that neither side of a pair is supported by its own stored label or by the stored labels of its series.

usage: judge_collisions.py <name_conflicts.csv> <out.csv>
"""
import collections as co
import csv
import sys

from name_judge import Context, Exclusion, Judge


def main():
    src, out = sys.argv[1], sys.argv[2]
    with open(src, encoding='utf-8', newline='') as f:
        lines = [r for r in csv.DictReader(f) if r['namespace'] == 'move' and r['rehashes'] == 'yes']
    by_hash = co.defaultdict(list)
    for r in lines:
        by_hash[r['hash']].append(r)
    by_hash = {h: rs for h, rs in by_hash.items() if len(rs) > 1}
    hidden = sorted({r['candidate_name'] for rs in by_hash.values() for r in rs})
    judge = Judge.load()
    exclusion = Exclusion(judge.kb, hidden)
    rows = []
    for h, rs in sorted(by_hash.items()):
        located = judge.rows_of(int(h, 16))
        verdicts = judge.judge([r['candidate_name'] for r in rs], Context(judge.kb, located, exclusion), exclusion=exclusion)
        info = {r['candidate_name']: r for r in rs}
        for v in verdicts:
            r = info[v['name']]
            top = sorted(v['contributions'].items(), key=lambda kv: -abs(kv[1]))[:4]
            rows.append(dict(hash=h, candidate_name=v['name'], judge_rank=v['rank'], judge_score='%.3f' % v['score'],
                             judge_margin_to_other='%.3f' % v['margin'], judge_log_likelihood_ratio='%.3f' % v['log_likelihood_ratio'],
                             move_keys_agreeing=r['move_keys_agreeing'], move_keys_of_the_hash=r['move_keys_of_the_hash'],
                             sources=r['sources'], judge_main_reasons=' | '.join('%s %+.2f' % kv for kv in top)))
    with open(out, 'w', encoding='utf-8', newline='') as f:
        w = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator='\n'); w.writeheader(); w.writerows(rows)
    agree = sum(1 for r in rows if r['judge_rank'] == 1 and int(r['move_keys_agreeing']) > 0)
    print('collided move hashes judged:', len(by_hash), '| judge puts the key-confirmed name first in', agree)
    for r in rows:
        print(r['hash'], r['judge_rank'], '%-24s' % r['candidate_name'], 'score', r['judge_score'], 'keys', r['move_keys_agreeing'], 'of', r['move_keys_of_the_hash'])


if __name__ == '__main__':
    main()
