"""Row context of a candidate move name: a second opinion that uses nothing of the search.

    C = Context(keys)                  keys = move_key.load_keys()
    parts, like = C.of(name, hash)

    parts   the parts of the name (split at underscores, three characters or more) that are also parts of the
            verified animation name of a row of the hash
    like    True when a named move within three rows (on any list of the hash) starts with the same head as the
            name: the text up to and including the first underscore, at least three characters
"""
from name_judge_data import load_all_overlays, load_t8_rows


class Context:
    def __init__(self, keys):
        self.keys = keys; self.accepted = load_all_overlays(); self.rows = {}
        for r in load_t8_rows():
            self.rows.setdefault(r['code'], {})[int(r['id'])] = r

    def of(self, name, value):
        parts = [t.lower() for t in name.split('_') if t]; head = name.split('_')[0]+'_' if '_' in name else ''; shared = set(); like = False
        for code, i, _ in self.keys.get(value, []):
            r = self.rows.get(code, {}).get(i)
            if r and r['anim_status'] == 'verified' and r['anim_name']:
                anim = [t.lower() for t in r['anim_name'].split('_') if t]; shared |= {t for t in parts if len(t) >= 3 and t in anim}
            for j in range(i-3, i+4):
                q = self.rows.get(code, {}).get(j)
                if q and j != i:
                    known = q['move_name'] if q['move_status'] == 'verified' else self.accepted.get(q['move_hash'], '')
                    like = like or bool(known and len(head) >= 3 and known.startswith(head))
        return sorted(shared), like
