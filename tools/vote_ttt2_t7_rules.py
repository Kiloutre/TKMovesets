"""Votes Tag Tournament 2 -> Tekken 7 requirement and property ids, from characters present in both games.

TTT2 side: folders saved by the old Python TekkenMovesetExtractor (`tag2_*`, `rpcs3_tag2_*`: raw TTT2 ids).
T7 side: payload dumps of T7 movesets built from the game's files
(`TKMovesets2.exe --headless dump-payload-t8 T7_<name>.tkmvst <dir>/T7_<name>.bin > <dir>/T7_<name>.json`).

Moves are paired by name. In each pair of moves, these lists are paired, and each paired record votes TTT2 id -> T7 id:
- extra properties, aligned on their starting frames;
- cancels, paired by target move name and frame windows; their requirement lists vote position by position when
  both have the same length; group cancels (0x800b) are followed the same way;
- hit conditions, position by position when both lists have the same length, with their requirement lists.
Each id's votes are compared with the converter's current rule (Converter_ttt2_to_t7_Aliases.cpp, with the gap filling
of Aliases::BuildAliasDictionary; ids without a rule pass through unchanged).

Accepted rules (--emit): at least 10 votes from 3 characters, 80% agreeing. A parameter value is converted when at least
2 characters agree on it and none disagrees. "Player (not) character id" checks keep their values: the importer
compares them with the file's own character id, which stays in TTT2 numbering.

Usage: python -I tools/vote_ttt2_t7_rules.py EXTRACTED_CHARS T7_DUMP_DIR OUTPUT.json [--emit Converter_ttt2_to_t7_Votes.inc]
"""
import collections
import difflib
import glob
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ALIASES_CPP = os.path.join(HERE, '..', 'TKMovesets2', 'GameSpecific', 'MovesetConverters', 'Converter_ttt2_to_t7_Aliases.cpp')
T7_END_REQ, TTT2_END_REQ = 881, 690
END_CANCEL, END_GROUP, GROUP_CANCEL = 0x8000, 0x800C, 0x800B

# -- Current rules -- #

def current_rules():
    text = open(ALIASES_CPP, encoding='utf-8').read()
    body = text[text.index('InitAliasDictionary'):text.index('namespace TTT2_T7_Aliases')]
    rules, nofill = {}, set()
    for match in re.finditer(r'\{\s*(0x[0-9a-fA-F]+|\d+)\s*,\s*\{\s*\.target_id\s*=\s*(0x[0-9a-fA-F]+|\d+)([^\n]*)', body):
        key, target = int(match.group(1), 0), int(match.group(2), 0)
        rules[key] = target
        if 'nofill' in match.group(3): nofill.add(key)
    keys = sorted(rules)
    filled = dict(rules)
    for key, following in zip(keys, keys[1:]):  # Aliases::BuildAliasDictionary
        if key in nofill or following in nofill: continue
        if rules[key] - key == rules[following] - following:
            for k in range(key + 1, following): filled[k] = rules[key] + (k - key)
    return rules, filled

# -- TTT2 (extractor JSON) -- #

class TTT2:
    def __init__(self, folder):
        path = next(p for p in glob.glob(os.path.join(folder, '*.json')))
        m = json.load(open(path))
        self.m = m
        self.names = [x['name'] for x in m['moves']]
        self.aliases = m['aliases'][56:112]

    def target(self, move_id):
        if move_id >= 0x8000:
            k = move_id - 0x8000
            if k >= len(self.aliases): return None
            move_id = self.aliases[k]
        return self.names[move_id] if move_id < len(self.names) else None

    def requirements(self, index):
        out, reqs = [], self.m['requirements']
        while 0 <= index < len(reqs):
            out.append((reqs[index]['req'], reqs[index]['param']))
            if reqs[index]['req'] == TTT2_END_REQ: break
            index += 1
        return out

    def cancels(self, index, group=False):
        out, items = [], self.m['group_cancels' if group else 'cancels']
        end = END_GROUP if group else END_CANCEL
        while 0 <= index < len(items):
            c = items[index]
            out.append({'command': c['command'], 'req': c['requirement_idx'], 'target': c['move_id'],
                        'window': (c['frame_window_start'], c['frame_window_end'], c['starting_frame'])})
            if c['command'] == end: break
            index += 1
        return out

    def properties(self, index):
        out, props = [], self.m['extra_move_properties']
        while 0 <= index < len(props):
            p = props[index]
            out.append((p['type'], p['id'], p['value']))
            if p['id'] == 0: break
            index += 1
        return out

    def hit_conditions(self, index):
        out, hits = [], self.m['hit_conditions']
        while 0 <= index < len(hits):
            h = hits[index]
            reqs = self.requirements(h['requirement_idx'])
            out.append({'req': h['requirement_idx'], 'damage': h['damage']})
            if reqs and reqs[0][0] == TTT2_END_REQ: break
            index += 1
        return out

    def moves(self):
        for i, x in enumerate(self.m['moves']):
            yield x['name'], {'cancel': x['cancel_idx'], 'hit': x['hit_condition_idx'], 'prop': x['extra_properties_idx'],
                              'hitbox': x['hitbox_location']}

# -- T7 (payload dump) -- #

SIZES = [0x70, 8, 0x18, 0xa8, 0x10, 2, 0x28, 0x28, 4, 0xc, 0x10, 0x10, 0xb0, 4, 0x10, 8, 4, 0xc, 0x10]
NONE = 2 ** 64 - 1

class T7:
    def __init__(self, path):
        self.b = open(path, 'rb').read()
        blocks = json.load(open(path[:-4] + '.json'))['blocks']
        self.names_at, self.mb = blocks[3], blocks[4]
        self.table = [struct.unpack_from('<QQ', self.b, blocks[1] + 16 * i) for i in range(19)]
        self.aliases = struct.unpack_from('<56H', self.b, 0x98)
        self.move_count = self.table[12][1]
        self.names = [self.string(struct.unpack_from('<Q', self.b, self.record(12, i))[0]) for i in range(self.move_count)]

    def string(self, offset):
        s = self.names_at + offset
        return self.b[s:self.b.index(b'\0', s)].decode('latin1')

    def record(self, table, index):
        return self.mb + self.table[table][0] + index * SIZES[table]

    def valid(self, table, index):
        return index != NONE and index < self.table[table][1]

    def target(self, move_id):
        if move_id >= 0x8000:
            k = move_id - 0x8000
            if k >= len(self.aliases): return None
            move_id = self.aliases[k]
        return self.names[move_id] if move_id < len(self.names) else None

    def requirements(self, index):
        out = []
        while self.valid(1, index):
            c, p = struct.unpack_from('<II', self.b, self.record(1, index))
            out.append((c, p))
            if c == T7_END_REQ: break
            index += 1
        return out

    def cancels(self, index, group=False):
        out, table = [], 7 if group else 6
        end = END_GROUP if group else END_CANCEL
        while self.valid(table, index):
            o = self.record(table, index)
            command, req, _, ws, we, sf, target = struct.unpack_from('<QQQIIIH', self.b, o)
            out.append({'command': command, 'req': req, 'target': target, 'window': (ws, we, sf)})
            if command == end: break
            index += 1
        return out

    def properties(self, index):
        out = []
        while self.valid(9, index):
            f, i, v = struct.unpack_from('<III', self.b, self.record(9, index))
            out.append((f, i, v))
            if i == 0: break
            index += 1
        return out

    def hit_conditions(self, index):
        out = []
        while self.valid(2, index):
            req, damage = struct.unpack_from('<QI', self.b, self.record(2, index))
            out.append({'req': req, 'damage': damage})
            reqs = self.requirements(req)
            if reqs and reqs[0][0] == T7_END_REQ: break
            index += 1
        return out

    def moves(self):
        for i in range(self.move_count):
            o = self.record(12, i)
            cancel, = struct.unpack_from('<Q', self.b, o + 0x20)
            hit, = struct.unpack_from('<Q', self.b, o + 0x60)
            prop, = struct.unpack_from('<Q', self.b, o + 0x80)
            hitbox, = struct.unpack_from('<I', self.b, o + 0x9c)
            yield self.names[i], {'cancel': cancel, 'hit': hit, 'prop': prop, 'hitbox': hitbox}

# -- Votes -- #

class Votes:
    def __init__(self):
        self.ids = collections.defaultdict(collections.Counter)     # ttt2 id -> t7 id -> votes
        self.chars = collections.defaultdict(lambda: collections.defaultdict(set))
        self.params = collections.defaultdict(collections.Counter)  # (ttt2 id, t7 id) -> (param, param) -> votes
        self.value_chars = collections.defaultdict(lambda: collections.defaultdict(set))  # ... param -> t7 param -> characters

    def add(self, kind, source, target, sparam, tparam, character):
        key = (kind, source)
        self.ids[key][target] += 1
        self.chars[key][target].add(character)
        self.params[(kind, source, target)][(sparam, tparam)] += 1
        self.value_chars[(kind, source, target, sparam)][tparam].add(character)

def aligned_pairs(source_ids, target_ids, mapping):
    """Positions paired by aligning the TTT2 ids, mapped with the rules known so far, on the T7 ids: matching runs pair,
    and so do runs that differ but have the same length (where an unknown or wrong rule shows)."""
    mapped = [mapping.get(x, x) for x in source_ids]
    matcher = difflib.SequenceMatcher(None, mapped, target_ids, autojunk=False)
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == 'equal' or (tag == 'replace' and i2 - i1 == j2 - j1):
            for k in range(i2 - i1): yield i1 + k, j1 + k

def vote_requirements(votes, a, b, character, mapping):
    for i, j in aligned_pairs([r[0] for r in a], [r[0] for r in b], mapping):
        votes.add('requirement', a[i][0], b[j][0], a[i][1], b[j][1], character)

def pair_cancels(src, dst, src_char, dst_char):
    """Pairs cancels of two lists: same target move and frame windows first, then same target move."""
    pairs, used = [], set()
    for strict in (True, False):
        for i, a in enumerate(src):
            if any(i == p[0] for p in pairs): continue
            ta = src_char.target(a['target']) if a['command'] not in (END_CANCEL, END_GROUP, GROUP_CANCEL) else a['command']
            for j, b in enumerate(dst):
                if j in used: continue
                tb = dst_char.target(b['target']) if b['command'] not in (END_CANCEL, END_GROUP, GROUP_CANCEL) else b['command']
                if ta is None or ta != tb: continue
                if strict and a['window'] != b['window']: continue
                pairs.append((i, j)); used.add(j); break
    return [(src[i], dst[j]) for i, j in pairs]

def compare(votes, ttt2, t7, character, mapping):
    t7_moves = {}
    for name, move in t7.moves(): t7_moves.setdefault(name, move)
    paired = 0
    for name, a in ttt2.moves():
        b = t7_moves.get(name)
        if not b: continue
        paired += 1
        # Extra properties: the properties starting on the same frame in both lists, aligned on their ids (several
        # properties on one frame may be ordered differently)
        pa = [p for p in (ttt2.properties(a['prop']) if a['prop'] >= 0 else []) if p[1]]
        pb = [p for p in (t7.properties(b['prop']) if b['prop'] != NONE else []) if p[1]]
        for frame in sorted(set(p[0] for p in pa) & set(p[0] for p in pb)):
            ga = [p for p in pa if p[0] == frame]; gb = [p for p in pb if p[0] == frame]
            for i, j in aligned_pairs([p[1] for p in ga], [p[1] for p in gb], mapping):
                votes.add('property', ga[i][1], gb[j][1], ga[i][2], gb[j][2], character)
        # Cancels and their requirement lists, group cancels
        for ca, cb in pair_cancels(ttt2.cancels(a['cancel']), t7.cancels(b['cancel']), ttt2, t7):
            vote_requirements(votes, ttt2.requirements(ca['req']), t7.requirements(cb['req']), character, mapping)
            if ca['command'] == GROUP_CANCEL and cb['command'] == GROUP_CANCEL:
                for ga, gb in pair_cancels(ttt2.cancels(ca['target'], True), t7.cancels(cb['target'], True), ttt2, t7):
                    vote_requirements(votes, ttt2.requirements(ga['req']), t7.requirements(gb['req']), character, mapping)
        # Hitbox bytes (each byte names a limb)
        for sb, tb in zip(struct.pack('<I', a['hitbox']), struct.pack('<I', b['hitbox'])):
            votes.add('hitbox', sb, tb, 0, 0, character)
        # Hit conditions
        ha = ttt2.hit_conditions(a['hit']) if a['hit'] >= 0 else []
        hb = t7.hit_conditions(b['hit']) if b['hit'] != NONE else []
        if len(ha) == len(hb):
            for x, y in zip(ha, hb):
                vote_requirements(votes, ttt2.requirements(x['req']), t7.requirements(y['req']), character, mapping)
    return paired

def values(votes, kind, source, target):
    """Per TTT2 parameter: the T7 parameter most characters agree on, with how many agree and disagree."""
    out = []
    for (k, s, t, sparam), targets in votes.value_chars.items():
        if (k, s, t) != (kind, source, target): continue
        ranked = sorted(targets.items(), key=lambda kv: -len(kv[1]))
        best, chars = ranked[0]
        others = set().union(*(c for _, c in ranked[1:])) - chars if len(ranked) > 1 else set()
        out.append([sparam, best, len(chars), len(others)])
    out.sort(key=lambda v: -v[2])
    return out

OWN_CHARACTER_CHECKS = {217, 218}  # T7 "player character id", "player not character id"

def decide(ids, min_votes=10, min_agree=0.8, min_chars=3, value_chars=2):
    rules = []
    for x in ids:
        if x['kind'] == 'hitbox': continue
        if x['best_votes'] < min_votes or x['agreement'] < min_agree or x['characters'] < min_chars: continue
        table = {} if x['best'] in OWN_CHARACTER_CHECKS else {s: t for s, t, c, o in x['values'] if s != t and c >= value_chars and o == 0}
        if x['best'] == x['current'] and not table: continue
        rules.append({'kind': x['kind'], 'ttt2': x['ttt2'], 't7': x['best'], 'current': x['current'], 'params': table,
                      'votes': x['best_votes'], 'of': x['votes'], 'chars': x['characters']})
    return rules

def decide_hitbox(ids, min_agree=0.8, min_chars=3):
    # Hitbox bytes that change, by the same thresholds as ids (votes count every use, so no minimum is needed)
    return [{'ttt2': x['ttt2'], 't7': x['best'], 'votes': x['best_votes'], 'of': x['votes'], 'chars': x['characters']}
            for x in sorted(ids, key=lambda x: x['ttt2']) if x['kind'] == 'hitbox' and x['best'] != x['ttt2']
            and x['agreement'] >= min_agree and x['characters'] >= min_chars]

def emit(rules, path, hitbox=()):
    fmt = lambda kind, v: hex(v) if kind == 'property' else str(v)
    lines = ['// Generated by tools/vote_ttt2_t7_rules.py from TTT2 and T7 movesets of the same characters: do not edit.',
             '// Applied over InitAliasDictionary: TTT2 id, T7 id, parameter count; then the parameters, in the same order.',
             'static const uint32_t VotedRules[][3] = {']
    for x in rules:
        note = 'was %s, %d/%d votes, %d characters' % (fmt(x['kind'], x['current']), x['votes'], x['of'], x['chars'])
        lines.append('    { %s, %s, %d }, // %s %s' % (fmt(x['kind'], x['ttt2']), fmt(x['kind'], x['t7']), len(x['params']), x['kind'], note))
    lines += ['};', 'static const uint32_t VotedParams[][2] = {']
    for x in rules:
        for sp, tp in sorted(x['params'].items()): lines.append('    { %d, %d }, // %s' % (sp, tp, fmt(x['kind'], x['ttt2'])))
    if not any(x['params'] for x in rules): lines.append('    { 0, 0 },')
    lines.append('};')
    lines.append('// Hitbox bytes: TTT2 byte, T7 byte')
    lines.append('static const Byte VotedHitboxBytes[][2] = {')
    for x in hitbox: lines.append('    { 0x%02x, 0x%02x }, // %d/%d votes, %d characters' % (x['ttt2'], x['t7'], x['votes'], x['of'], x['chars']))
    if not hitbox: lines.append('    { 0, 0 },')
    lines.append('};')
    with open(path, 'w', encoding='utf-8', newline=chr(10)) as out:
        out.write(chr(10).join(lines) + chr(10))

def collect(extracted, t7_files, mapping, report=None):
    votes = Votes()
    for folder in sorted(glob.glob(os.path.join(extracted, 'tag2_*')) + glob.glob(os.path.join(extracted, 'rpcs3_tag2_*'))):
        base = os.path.basename(folder)
        name = re.sub(r'^(rpcs3_)?tag2_', '', base).replace('_', ' ').lower()
        if name not in t7_files: continue
        ttt2, t7 = TTT2(folder), T7(t7_files[name])
        paired = compare(votes, ttt2, t7, base, mapping)
        if report is not None:
            report['pairs'].append({'ttt2': base, 't7': os.path.basename(t7_files[name]), 'moves': len(ttt2.names), 'paired_moves': paired})
    return votes

def summarize(votes, rules, filled):
    ids = []
    for (kind, source), counter in votes.ids.items():
        total = sum(counter.values())
        best, count = counter.most_common(1)[0]
        current = source if kind == 'hitbox' else filled.get(source, source)
        params = votes.params[(kind, source, best)]
        same_param = sum(n for (sp, tp), n in params.items() if sp == tp)
        ids.append({
            'kind': kind, 'ttt2': source, 'votes': total, 'best': best, 'best_votes': count,
            'agreement': round(count / total, 3), 'characters': len(votes.chars[(kind, source)][best]),
            'current': current, 'current_votes': counter.get(current, 0), 'explicit_rule': source in rules,
            'others': counter.most_common(4)[1:],
            'param_same': round(same_param / sum(params.values()), 3),
            'param_pairs': [[sp, tp, n] for (sp, tp), n in params.most_common(6)],
            'values': values(votes, kind, source, best),
        })
    ids.sort(key=lambda x: (x['kind'], -x['votes']))
    return ids

def main(extracted, t7_dir, output, *options):
    rules, filled = current_rules()
    t7_files = {os.path.basename(p)[3:-4].lower(): p for p in glob.glob(os.path.join(t7_dir, 'T7_*.bin'))}
    # Pass 1 aligns lists with the current rules, pass 2 with the rules pass 1 accepted
    mapping = dict(filled)
    for x in decide(summarize(collect(extracted, t7_files, mapping), rules, filled)): mapping[x['ttt2']] = x['t7']
    report = {'pairs': []}
    votes = collect(extracted, t7_files, mapping, report)
    ids = summarize(votes, rules, filled)
    report['ids'] = ids
    # Every vote, to score a rule set: [kind, ttt2 id, t7 id, ttt2 param, t7 param, count]
    report['pairs_detail'] = [[k, s_, t, sp, tp, n] for (k, s_, t), c in votes.params.items() for (sp, tp), n in c.items()]
    report['current_rules'] = {str(k): v for k, v in filled.items()}
    report['rules'] = decide(ids)
    report['hitbox'] = decide_hitbox(ids)
    if '--emit' in options: emit(report['rules'], options[options.index('--emit') + 1], report['hitbox'])
    json.dump(report, open(output, 'w'))

if __name__ == '__main__':
    main(*sys.argv[1:])
