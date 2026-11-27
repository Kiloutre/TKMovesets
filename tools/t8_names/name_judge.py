"""Name judge: grade same-hash candidate names for one T8 move hash.

Given a list of names that all hash to one retail T8 move hash, plus that hash's
context (owner codes, rows, neighbour labels, animation names, same-animation
moves, the aligned older-game window and the move's own structure), return for
every name a score, a calibrated probability and the readable reasons that fired.

It accepts nothing. A hash match plus a good-looking spelling is not evidence;
the score says how much better a name fits than a hash-collision match from a
wide search would. Moves only; animations are a separate namespace and are not
handled here.

Import:
    from name_judge import Judge
    judge = Judge.load()                       # all verified and owner-accepted names + frozen model
    verdicts = judge.judge_hash(0x8C4E7CA1, ['sDm_tohtaiR42_DMGRATE', 'Ak_Memai42_DMGMLb_'])

Command line:
    python name_judge.py --hash 0x8C4E7CA1 NAME [NAME ...]
    python name_judge.py --hash 0x8C4E7CA1 --file candidates.txt --list-type 100000+stems --list-size 240

No leakage rule: the knowledge base is a sum of per-name contributions, so a
held-out name is removed exactly by subtracting its own contributions (class
Exclusion). Nothing else in this module remembers a name.
"""
import t8names_paths as _P
import bisect
import collections as co
import gzip
import json
import math
import re
import sys
from pathlib import Path

HERE = _P.LAB
TERMINALS = frozenset(('f', 'y', 'n', 'c', 'l', 'r', 'e', 'end', 'start', 'loop', 'cancel', 'ex', 'cs', 'sp'))
TOKEN = re.compile(r'[0-9]+|[A-Z]+(?![a-z])|[A-Z]?[a-z]+|_+|[^A-Za-z0-9_]+')
DIGITS = re.compile(r'[0-9]+')
BUTTON_UP = re.compile(r'(?<![A-Za-z])([1-9]{0,4})((?:LP|RP|LK|RK|WP|WK)+)(?![a-z])')
BUTTON_LO = re.compile(r'(?<![A-Za-z])([1-9]{0,4})((?:lp|rp|lk|rk|wp|wk)+)(?![A-Za-z])')
BUTTON_MASK = {'LP': 1, 'RP': 2, 'LK': 4, 'RK': 8, 'WP': 3, 'WK': 12}
ROMAJI = re.compile(r'^(?:n(?![aiueoy])|([kstpgzdbcfhmr])(?=\1)|(?:ch|sh|ts|[kgsztdnhbpmyrwjfv]y?)?[aiueo])+$')
LM_ORDER = 5
PAD = '^'*LM_ORDER
SUFFIX_LENGTHS = (4, 5, 6, 7, 8, 10)
STRUCT_HEAD_MIN = 12
MAX_SAME_ANIM = 48
MAX_WINDOW = 40
TOP_LISTS = 3


# ----------------------------------------------------------------------------
# String helpers
# ----------------------------------------------------------------------------

def tokens(name):
    """[(text, start, end)] covering the whole name."""
    return [(m.group(), m.start(), m.end()) for m in TOKEN.finditer(name)]


def split_head(name):
    if '_' not in name:
        return '', name
    head, body = name.split('_', 1)
    return head+'_', body


def digitnorm(name):
    return DIGITS.sub('#', name)


def strip_variant(name):
    """(base, variant) when the last underscore segment is a terminal variant, else (name, '')."""
    parts = name.split('_')
    if len(parts) > 2 and parts[-1].lower() in TERMINALS:
        return '_'.join(parts[:-1]), parts[-1]
    return name, ''


def family(name):
    """Same key as move_pipeline.move_family: ignore owner head, digits, terminal variants."""
    body = split_head(name)[1].lower() if '_' in name else name.lower()
    body = DIGITS.sub('', body)
    parts = [p for p in body.split('_') if p]
    while parts and parts[-1] in TERMINALS:
        parts.pop()
    return '_'.join(parts) or '<empty>'


def shape(text):
    """Collapsed character-class shape: 'DMGRATE' -> 'U', 'Wall2b' -> 'Uldl'."""
    out = []
    for ch in text:
        c = 'd' if ch.isdigit() else 'U' if ch.isupper() else 'l' if ch.islower() else ch
        if not out or out[-1] != c:
            out.append(c)
    return ''.join(out)


def classes(text):
    """Character classes, one per character: 'Wall2' -> 'Ullld'."""
    return ''.join('d' if ch.isdigit() else 'U' if ch.isupper() else 'l' if ch.islower() else ch for ch in text)


def lcp(a, b):
    n = min(len(a), len(b)); i = 0
    while i < n and a[i] == b[i]:
        i += 1
    return i


def lcs(a, b):
    n = min(len(a), len(b)); i = 0
    while i < n and a[-1-i] == b[-1-i]:
        i += 1
    return i


def command_tokens(name):
    """[(digits, [button, ...], start, end)] for command notation inside a name, upper or lower case."""
    out = []
    for pattern in (BUTTON_UP, BUTTON_LO):
        for m in pattern.finditer(name):
            run = m.group(2).upper()
            out.append((m.group(1), [run[i:i+2] for i in range(0, len(run), 2)], m.start(), m.end()))
    out.sort(key=lambda item: item[2])
    return out


def name_contributions(name):
    """Count-table contributions of ONE unique name, shared by both vocabularies.

    Returns [(table, key)]; every table is a plain count over unique names.
    """
    out = [('name', name)]
    history = PAD
    for ch in name+'$':
        for order in range(0, LM_ORDER+1):
            context = history[len(history)-order:] if order else ''
            out.append(('ng', context+'\x00'+ch)); out.append(('ngt', context))
        history = (history+ch)[-LM_ORDER:]
    for k in SUFFIX_LENGTHS:
        if len(name) > k:
            out.append(('suf', name[-k:]))
    toks = [t for t, a, b in tokens(name)]
    for t in set(toks):
        out.append(('tok', t))
    for t in {t.lower() for t in toks if len(t) >= 2 and t[0] != '_'}:
        out.append(('tokl', t))
    out.append(('tshape', classes(name[-5:])))
    out.append(('ftok', toks[-1]))
    for a, b in zip(toks, toks[1:]):
        out.append(('tok2', a+'\x00'+b))
        if b.isdigit():
            out.append(('tokd', a+'\x00'+str(len(b))))
    segments = name.split('_')
    for s in set(segments):
        out.append(('seg', s))
    out.append(('fseg', segments[-1]))
    if len(segments) > 1:
        out.append(('fseg2', segments[-2]+'_'+segments[-1]))
    out.append(('fshape', shape(segments[-1])))
    out.append(('dn', digitnorm(name)))
    base, variant = strip_variant(name)
    out.append(('base', base))
    head, body = split_head(name)
    out.append(('head', head)); out.append(('body', body)); out.append(('len', len(name)))
    out.append(('headlen', head+'\x00'+str(len(name))))
    for k in (4, 6, 8, 10, 12):
        if len(name) >= k:
            out.append(('pre', name[:k]))
    return out


# ----------------------------------------------------------------------------
# Move structure atoms
# ----------------------------------------------------------------------------

def direction_digit(lo):
    """Direction bit field -> notation digit(s); bit d set means numpad direction d."""
    lo &= 0x3FE
    if not lo:
        return ''
    return ''.join(str(d) for d in range(1, 10) if lo >> d & 1 and d != 5)


def decode_command(edge):
    """(digits, button mask) of one incoming cancel edge [lo, hi, source, group, sequence]."""
    lo, hi, source, group, sequence = edge
    if sequence:
        digits = ''.join(direction_digit(step[0])[:1] for step in sequence if step[0] & 0x3DE)
        return digits, sequence[-1][1] & 0xF
    if lo >= 0x8000:
        return '', hi & 0xF
    return direction_digit(lo), hi & 0xF


def bucket_value(value, edges):
    return bisect.bisect_right(edges, value)


def structure_atoms(move):
    """Discrete, name-free facts about one move (dict from name_judge_data.load_structure)."""
    inc = move['inc']
    button = any(e[1] & 0xF for e in inc)
    direction = any((e[0] and e[0] < 0x8000 and e[0] & 0x3DE) or e[4] for e in inc)
    atoms = ['in:'+('btn' if button else 'dir' if direction else 'auto' if inc else 'none'),
             'rx:%d' % bool(move['rx']), 'hit:%d' % bool(move['hl']), 'dmg:%d' % bucket_value(move['dmg'], (1, 15, 30)),
             'trsrc:%d' % bool(move['trsrc']), 'out:%d' % bucket_value(move['nout'], (2, 5, 12, 30)),
             'air:%d' % bool(any(move['air'])), 'voice:%d' % bool(move['voice']),
             'su:%d' % bucket_value(move['su'], (1, 10, 14, 18, 25)), 'rc:%d' % bucket_value(move['rc'], (1, 20, 35, 60)),
             'nx:%d' % bucket_value(move['nx'], (1, 3, 8)), 'ns:%d' % bool(move['ns']), 'ne:%d' % bool(move['ne']),
             'hlv:%X' % (move['hl'] >> 8 & 0xFF if move['hl'] else 0), 'vu:%X' % (move['vu'] & 0xF),
             'amax:%d' % bucket_value(move['amax'], (1, 20, 40, 70, 120))]
    return atoms


def slim_move(move):
    """Compact form kept in memory: atoms, decoded incoming commands, outgoing targets, transition sources."""
    incoming = []
    for edge in move['inc'][:16]:
        digits, mask = decode_command(edge)
        incoming.append((sys.intern(digits), mask, edge[2]))
    return (tuple(sys.intern(a) for a in structure_atoms(move)), tuple(incoming),
            tuple(e[2] for e in move['out'][:30] if e[2] >= 0 and e[2] != move['id']), tuple(move['trsrc']))


def name_keys(name):
    """Name-side keys whose relation to structure is learned: head, final segment, coarse flags."""
    head, body = split_head(name); segments = name.split('_'); last = segments[-1]
    commands = command_tokens(name)
    return dict(head=head or '<bare>', fseg=last if len(segments) > 1 else '<bare>',
                cmd='cmd' if commands else 'nocmd',
                var=last.lower() if len(segments) > 2 and last.lower() in TERMINALS else '-')


# ----------------------------------------------------------------------------
# Knowledge base
# ----------------------------------------------------------------------------

class Tables:
    def __init__(self):
        self.t = co.defaultdict(dict)

    def add(self, table, key, weight=1):
        d = self.t[table]; d[key] = d.get(key, 0)+weight

    def get(self, table, key):
        return self.t[table].get(key, 0)


class Exclusion:
    """Contributions of the names hidden for one case; subtracted at every lookup."""
    def __init__(self, kb, names=()):
        self.names = frozenset(names); self.t8 = Tables(); self.all = Tables(); self.struct = Tables()
        for name in self.names:
            if name in kb.t8_names:
                for table, key in name_contributions(name):
                    self.t8.add(table, key)
                for code in kb.t8_names[name]:
                    self.t8.add('headcode', code+'\x00'+split_head(name)[0]); self.t8.add('codetot', code)
                    self.t8.add('lencode', code+'\x00'+str(len(name)))
                self.t8.add('ncodes', kb.ncodes_key(len(kb.t8_names[name]))+'\x00'+split_head(name)[0])
                self.t8.add('ncodestot', kb.ncodes_key(len(kb.t8_names[name])))
                for row_atoms in kb.name_rows_atoms.get(name, ()):
                    kb.add_struct(self.struct, name, row_atoms)
            if name in kb.all_names:
                for table, key in name_contributions(name):
                    self.all.add(table, key)


class KnowledgeBase:
    """Count tables over visible verified T8 move names, and over T8 + older games.

    t8_rows: iterable of dicts with code, id, move_name ('' when hidden), anim_name ('' when not verified).
    legacy_lists: {list key: [move name, ...]} in list order (T7 / Tag2 / T6 / T5 / T4).
    structure: name_judge_data.load_structure() or None.
    hidden: names that must not enter any table (also removed from the older-game vocabulary).
    """
    def __init__(self, t8_rows, legacy_lists, structure=None, hidden=(), hidden_families=()):
        hidden = set(hidden); hidden_families = set(hidden_families)
        self.rows = {}; self.by_code = co.defaultdict(dict); self.t8_names = co.defaultdict(set)
        self.anim_moves = co.defaultdict(set); self.name_rows = co.defaultdict(list)
        for r in t8_rows:
            code = r['code']; identifier = int(r['id']); name = r.get('move_name') or ''
            if name in hidden:
                name = ''
            anim = r.get('anim_name') or ''
            self.by_code[code][identifier] = (name, anim, r.get('move_hash'))
            if name:
                self.t8_names[name].add(code); self.name_rows[name].append((code, identifier))
                if anim:
                    self.anim_moves[anim].add(name)
        self.t8 = Tables(); self.all = Tables(); self.struct = Tables()
        for name, codes in self.t8_names.items():
            for table, key in name_contributions(name):
                self.t8.add(table, key)
            head = split_head(name)[0]
            for code in codes:
                self.t8.add('headcode', code+'\x00'+head); self.t8.add('codetot', code)
                self.t8.add('lencode', code+'\x00'+str(len(name)))
            self.t8.add('ncodes', self.ncodes_key(len(codes))+'\x00'+head); self.t8.add('ncodestot', self.ncodes_key(len(codes)))
        self.legacy_lists = {}
        legacy_names = set()
        for key, names in legacy_lists.items():
            clean = [n if (n and n not in hidden and (not hidden_families or family(n) not in hidden_families)) else '' for n in names]
            self.legacy_lists[key] = clean; legacy_names.update(n for n in clean if n and n.isascii())
        self.all_names = set(self.t8_names) | legacy_names
        for name in self.all_names:
            for table, key in name_contributions(name):
                self.all.add(table, key)
        self.legacy_only = legacy_names-set(self.t8_names)
        self.sorted_t8 = sorted(self.t8_names)
        self.structure = None; self.name_rows_atoms = {}
        if structure is not None:
            if 'atoms' in structure and 'codes' in structure:      # small cache (name_judge_data.load_structure_slim)
                atoms = [sys.intern(a) for a in structure['atoms']]
                self.structure = {code: [(tuple(atoms[i] for i in m[0]), tuple((sys.intern(e[0]), e[1], e[2]) for e in m[1]), tuple(m[2]), tuple(m[3]))
                                         for m in moves] for code, moves in structure['codes'].items()}
            else:
                self.structure = {code: [slim_move(m) for m in doc['moves']] for code, doc in structure.items()}
            for name, located in self.name_rows.items():
                rows_atoms = []
                for code, identifier in located[:6]:
                    move = self.move(code, identifier)
                    if move is not None:
                        rows_atoms.append(move[0])
                self.name_rows_atoms[name] = rows_atoms
                for atoms in rows_atoms:
                    self.add_struct(self.struct, name, atoms)
        self._alignments = {}
        self.vocab_size = max(40, len({k.split('\x00')[1] for k in self.t8.t['ng'] if k.startswith('\x00')}))

    @staticmethod
    def ncodes_key(n):
        return '1' if n == 1 else '2-5' if n <= 5 else '6-39' if n < 40 else '40+'

    @staticmethod
    def add_struct(tables, name, atoms):
        keys = name_keys(name)
        for kind, value in keys.items():
            key = kind+'='+value
            tables.add('sk', key)
            for atom in atoms:
                tables.add('ska', key+'\x00'+atom)
        tables.add('sk', '*')
        for atom in atoms:
            tables.add('ska', '*\x00'+atom)

    def move(self, code, identifier):
        """(atoms, incoming [(digits, button mask, source id)], outgoing target ids, transition sources) or None."""
        if self.structure is None or code not in self.structure:
            return None
        moves = self.structure[code]
        return moves[identifier] if 0 <= identifier < len(moves) else None

    def label(self, code, identifier):
        entry = self.by_code.get(code, {}).get(identifier)
        return entry[0] if entry else ''

    # -- older-game alignment -------------------------------------------------
    def alignment(self, code):
        """Top older lists aligned to this character's visible names (longest increasing run)."""
        if code in self._alignments:
            return self._alignments[code]
        known = sorted((identifier, entry[0]) for identifier, entry in self.by_code.get(code, {}).items() if entry[0])
        ranked = []
        for key, names in self.legacy_lists.items():
            positions = {}
            for position, name in enumerate(names):
                if name and name not in positions:
                    positions[name] = position
            sequence = [(identifier, positions[name]) for identifier, name in known if name in positions]
            if len(sequence) > 50:
                path = _increasing(sequence); ranked.append((len(path), key, path))
        ranked.sort(key=lambda item: (-item[0], item[1]))
        self._alignments[code] = ranked[:TOP_LISTS]
        return self._alignments[code]

    def legacy_window(self, code, identifier):
        """Older names between the two aligned names that bracket this id: {name: exact_position}."""
        out = {}
        for length, key, path in self.alignment(code):
            ids = [p[0] for p in path]; k = bisect.bisect_left(ids, identifier)
            if k == 0 or k == len(path):
                continue
            (a, la), (b, lb) = path[k-1], path[k]
            if ids[k] == identifier:
                # the target itself is an aligned point (leave-one-out case): bracket it with its two aligned neighbours
                if k+1 == len(path):
                    continue
                b, lb = path[k+1]
            if lb-la-1 > MAX_WINDOW or b-a-1 > MAX_WINDOW:
                continue
            positional = la+(identifier-a) if (b-a) == (lb-la) else None
            for position in range(la, lb+1):
                name = self.legacy_lists[key][position]
                if name and name.isascii():
                    out[name] = out.get(name, False) or position == positional
        return out


def _increasing(sequence):
    tails = []; tail_index = []; back = [None]*len(sequence)
    for i, (identifier, position) in enumerate(sequence):
        k = bisect.bisect_left(tails, position)
        if k == len(tails):
            tails.append(position); tail_index.append(i)
        else:
            tails[k] = position; tail_index[k] = i
        back[i] = tail_index[k-1] if k else None
    out = []; i = tail_index[-1] if tail_index else None
    while i is not None:
        out.append(sequence[i]); i = back[i]
    return out[::-1]


# ----------------------------------------------------------------------------
# Context of one target hash
# ----------------------------------------------------------------------------

class Context:
    """Everything known about one target hash except its name."""
    def __init__(self, kb, located, exclusion=None):
        """located: [(code, id)] rows that carry the hash."""
        hidden = exclusion.names if exclusion else frozenset()
        self.located = [(c, int(i)) for c, i in located]; self.codes = sorted({c for c, i in self.located})
        self.neighbors = {}; anims = []
        for code, identifier in self.located[:8]:
            for offset in (-2, -1, 1, 2):
                name = kb.label(code, identifier+offset)
                if name and name not in hidden:
                    self.neighbors.setdefault(name, offset)
            entry = kb.by_code.get(code, {}).get(identifier)
            if entry and entry[1] and entry[1] not in anims:
                anims.append(entry[1])
        self.anims = anims
        same = []
        for anim in anims:
            for name in sorted(kb.anim_moves.get(anim, ())):
                if name not in hidden and name not in same:
                    same.append(name)
        self.same_anim = same[:MAX_SAME_ANIM]
        self.legacy = {}
        for code, identifier in self.located[:3]:
            for name, exact in kb.legacy_window(code, identifier).items():
                if name not in hidden:
                    self.legacy[name] = self.legacy.get(name, False) or exact
        self.atoms = []; self.pred = {}; self.succ = {}; self.commands = []; self.has_structure = False
        for code, identifier in self.located[:4]:
            move = kb.move(code, identifier)
            if move is None:
                continue
            self.has_structure = True
            atoms, incoming, outgoing, transition_sources = move
            if not self.atoms:
                self.atoms = list(atoms)
            for digits, mask, source in incoming:
                if mask or digits:
                    self.commands.append((digits, mask))
                name = kb.label(code, source)
                if name and name not in hidden:
                    self.pred.setdefault(name, (digits, mask))
            for target in outgoing:
                name = kb.label(code, target)
                if name and name not in hidden:
                    self.succ.setdefault(name, True)
            for source in transition_sources:
                name = kb.label(code, source)
                if name and name not in hidden:
                    self.pred.setdefault(name, ('', 0))
        self.buttons = {mask for digits, mask in self.commands if mask}
        self.directions = {digits for digits, mask in self.commands if digits}
        self.anchor_groups = dict(nb=list(self.neighbors), sa=self.same_anim, lg=list(self.legacy),
                                  gr=list(self.pred)+[n for n in self.succ if n not in self.pred])
        self._prepared = {}
        for kind, names in self.anchor_groups.items():
            self._prepared[kind] = [(a, digitnorm(a), strip_variant(a)[0], split_head(a)[0], frozenset(t.lower() for t, s, e in tokens(a) if t[0] != '_'))
                                    for a in names]
        self.anim_tokens = frozenset(t.lower() for anim in anims for t, s, e in tokens(anim) if t[0] != '_' and len(t) > 1)
        self.anim_remainders = [a.split('_', 1)[1] if '_' in a else a for a in anims]

    def describe(self):
        return dict(codes=self.codes, rows=self.located[:8], neighbors=self.neighbors, animations=self.anims,
                    same_animation_moves=self.same_anim[:12], legacy_window=sorted(self.legacy)[:12],
                    predecessors=list(self.pred)[:8], successors=list(self.succ)[:8],
                    input_commands=sorted({d+_button_name(m) for d, m in self.commands})[:12], atoms=self.atoms)


def _button_name(mask):
    for name, value in BUTTON_MASK.items():
        if value == mask:
            return name
    return '+'.join(n for n, v in (('LP', 1), ('RP', 2), ('LK', 4), ('RK', 8)) if mask & v) if mask else ''


# ----------------------------------------------------------------------------
# Features
# ----------------------------------------------------------------------------

FEATURE_FAMILIES = {
    'tail_lm': ('lm8_tail', 'lmA_tail', 'lm8_end', 'lm8_worst', 'lmA_worst', 'tail_best'),
    'tail_tokens': ('suf8', 'sufA', 'tailtok8', 'tailtokA', 'ftokfinal8', 'fseg8', 'fsegA', 'fseg2', 'fshape', 'boundary',
                    'tailtok_n', 'tok2tail', 'tail_seg', 'tail_unknown_len', 'tshape8', 'tshapeA'),
    'whole_string': ('lm8_pre', 'alltok8', 'unknown_tokens', 'head8', 'headcode', 'len_code', 'body_other', 'segments',
                     'odd_sep', 'trail_us', 'romaji', 'headlen'),
    'siblings': ('sib8', 'sibA', 'base8', 'baseA', 'swap8', 'sib_delta'),
    'numbers': ('num_tail', 'num_prev_width', 'num_prev_value'),
    'commands': ('cmd_any', 'cmd_tail'),
    'context_neighbors': ('nb_head', 'nb_lcp', 'nb_sib', 'nb_edit', 'nb_suf', 'nb_append', 'nb_tailtok', 'nb_any'),
    'context_same_anim': ('sa_head', 'sa_lcp', 'sa_sib', 'sa_edit', 'sa_suf', 'sa_append', 'sa_tailtok', 'sa_any',
                          'anim_tailtok', 'anim_overlap', 'anim_lcp'),
    'context_legacy': ('lg_head', 'lg_lcp', 'lg_sib', 'lg_edit', 'lg_suf', 'lg_append', 'lg_tailtok', 'lg_any', 'lg_exact'),
    'context_copy': ('copy_any', 'copy_explained', 'copy_tail'),
    'context_owner': ('ncodes', 'head_ncodes'),
    'structure_graph': ('gr_head', 'gr_lcp', 'gr_sib', 'gr_edit', 'gr_suf', 'gr_append', 'gr_tailtok', 'gr_any', 'pred_cmd'),
    'structure_command': ('btn_agree', 'dir_agree'),
    'structure_class': ('st_head', 'st_fseg', 'st_cmd', 'st_var'),
}
FEATURES = tuple(f for names in FEATURE_FAMILIES.values() for f in names)
FEATURE_INDEX = {f: i for i, f in enumerate(FEATURES)}
EVIDENCE_GROUPS = {
    'string': ('tail_lm', 'tail_tokens', 'whole_string', 'siblings', 'numbers', 'commands'),
    'context': ('context_neighbors', 'context_same_anim', 'context_legacy', 'context_copy', 'context_owner'),
    'structure': ('structure_graph', 'structure_command', 'structure_class'),
}


class Scorer:
    """Feature extraction for one (knowledge base, context, exclusion)."""
    def __init__(self, kb, context, exclusion=None):
        self.kb = kb; self.context = context; self.ex = exclusion
        self.ex8 = exclusion.t8.t if exclusion else None
        self.exA = exclusion.all.t if exclusion else None
        self.exS = exclusion.struct.t if exclusion else None
        self._lm_cache = {}
        self._struct_cache = {}

    # counts with leave-out subtraction
    def c8(self, table, key):
        value = self.kb.t8.t[table].get(key, 0)
        if self.ex8 is not None and value:
            value -= self.ex8[table].get(key, 0) if table in self.ex8 else 0
        return value

    def cA(self, table, key):
        value = self.kb.all.t[table].get(key, 0)
        if self.exA is not None and value:
            value -= self.exA[table].get(key, 0) if table in self.exA else 0
        return value

    def cS(self, table, key):
        value = self.kb.struct.t[table].get(key, 0)
        if self.exS is not None and value:
            value -= self.exS[table].get(key, 0) if table in self.exS else 0
        return value

    def char_logp(self, which, history, ch):
        """log2 P(ch | history) by interpolated back-off over orders 0..LM_ORDER."""
        key = (which, history, ch)
        cached = self._lm_cache.get(key)
        if cached is not None:
            return cached
        count = self.c8 if which == 8 else self.cA
        p = 1.0/(self.kb.vocab_size+1)
        for order in range(0, LM_ORDER+1):
            context = history[len(history)-order:] if order else ''
            total = count('ngt', context)
            if total <= 0:
                break
            lam = total/(total+3.0)
            p = lam*count('ng', context+'\x00'+ch)/total+(1-lam)*p
        value = math.log2(p) if p > 0 else -40.0
        self._lm_cache[key] = value
        return value

    def lm_span(self, which, name, start):
        """Per-character log2 probabilities of name[start:] plus the end marker."""
        text = PAD+name; out = []
        for i in range(start, len(name)+1):
            ch = name[i] if i < len(name) else '$'
            out.append(self.char_logp(which, text[i:i+LM_ORDER], ch))
        return out

    def segmentable(self, token):
        """Can an unknown token be cut into pieces that are tokens of at least two known names (any game, any case)?"""
        text = token.lower(); n = len(text); reach = [False]*(n+1); reach[0] = True
        for i in range(n):
            if not reach[i]:
                continue
            for j in range(i+2, n+1):
                piece = text[i:j]
                if piece.isdigit() or self.cA('tokl', piece) >= 2:
                    reach[j] = True
        return reach[n]

    def tail_bits(self, name):
        """Cheap first look: log2 probability of the last four characters plus end, best of the two character models."""
        start = max(0, len(name)-4)
        return max(sum(self.lm_span(8, name, start)), sum(self.lm_span('A', name, start)))

    def struct_llr(self, key):
        """Naive-Bayes log2 likelihood ratio of the target's atoms given a name-side key."""
        cached = self._struct_cache.get(key)
        if cached is not None:
            return cached
        n = self.cS('sk', key); total = self.cS('sk', '*'); value = 0.0
        if n >= STRUCT_HEAD_MIN and total > 0:
            for atom in self.context.atoms:
                p = (self.cS('ska', key+'\x00'+atom)+0.5)/(n+1.0)
                q = (self.cS('ska', '*\x00'+atom)+0.5)/(total+1.0)
                value += math.log2(p/q)
        else:
            value = float('nan')
        self._struct_cache[key] = value
        return value

    def features(self, name):
        kb = self.kb; ctx = self.context; f = [0.0]*len(FEATURES); n = len(name); I = FEATURE_INDEX
        tail_start = max(0, n-4); toks = tokens(name); segments = name.split('_'); head, body = split_head(name)
        # --- tail language model
        span8 = self.lm_span(8, name, tail_start); spanA = self.lm_span('A', name, tail_start)
        f[I['lm8_tail']] = sum(span8); f[I['lmA_tail']] = sum(spanA); f[I['lm8_end']] = span8[-1]
        f[I['lm8_worst']] = min(span8); f[I['lmA_worst']] = min(spanA)
        f[I['tail_best']] = max(f[I['lm8_tail']], f[I['lmA_tail']])
        if tail_start > 0:
            pre = self.lm_span(8, name[:tail_start], 0)[:-1]
            f[I['lm8_pre']] = sum(pre)/len(pre)
        # --- tail tokens
        best8 = 0; bestA = 0
        for k in SUFFIX_LENGTHS:
            if n > k:
                if self.c8('suf', name[-k:]) > 0:
                    best8 = k
                if self.cA('suf', name[-k:]) > 0:
                    bestA = k
        f[I['suf8']] = best8; f[I['sufA']] = bestA
        tail_tokens = [(t, a, b) for t, a, b in toks if b > tail_start and t[0] != '_']
        counts8 = [self.c8('tok', t) for t, a, b in tail_tokens]; countsA = [self.cA('tok', t) for t, a, b in tail_tokens]
        f[I['tailtok8']] = min(counts8) if counts8 else -1; f[I['tailtokA']] = min(countsA) if countsA else -1
        f[I['tailtok_n']] = len(tail_tokens)
        f[I['ftokfinal8']] = self.c8('ftok', toks[-1][0]) if toks else 0
        f[I['fseg8']] = self.c8('fseg', segments[-1]); f[I['fsegA']] = self.cA('fseg', segments[-1])
        f[I['fseg2']] = self.c8('fseg2', segments[-2]+'_'+segments[-1]) if len(segments) > 2 else -1
        f[I['fshape']] = self.c8('fshape', shape(segments[-1]))
        f[I['boundary']] = 1.0 if any(a == tail_start for t, a, b in toks) else 0.0
        pairs = [self.c8('tok2', toks[i-1][0]+'\x00'+toks[i][0]) for i in range(1, len(toks)) if toks[i][2] > tail_start]
        f[I['tok2tail']] = min(pairs) if pairs else -1
        unknown = [t for (t, a, b), c8, cA in zip(tail_tokens, counts8, countsA) if c8 <= 0 and cA <= 0 and not t.isdigit()]
        f[I['tail_unknown_len']] = max((len(t) for t in unknown), default=0)
        f[I['tail_seg']] = (1.0 if all(self.segmentable(t) for t in unknown) else 0.0) if unknown else -1.0
        f[I['tshape8']] = self.c8('tshape', classes(name[-5:])); f[I['tshapeA']] = self.cA('tshape', classes(name[-5:]))
        # --- whole string
        real = [t for t, a, b in toks if t[0] != '_']
        all8 = [self.c8('tok', t) for t in real]
        f[I['alltok8']] = min(all8) if all8 else 0
        f[I['unknown_tokens']] = sum(1 for c in all8 if c <= 0)
        f[I['head8']] = self.c8('head', head)
        code_total = sum(self.c8('codetot', c) for c in ctx.codes[:6]) or 1
        f[I['headcode']] = sum(self.c8('headcode', c+'\x00'+head) for c in ctx.codes[:6])/code_total
        f[I['len_code']] = sum(self.c8('lencode', c+'\x00'+str(n)) for c in ctx.codes[:6])/code_total
        f[I['headlen']] = self.c8('headlen', head+'\x00'+str(n))
        f[I['body_other']] = self.c8('body', body)
        f[I['segments']] = len(segments)
        f[I['odd_sep']] = 1.0 if '__' in name or name.startswith('_') else 0.0
        f[I['trail_us']] = 1.0 if name.endswith('_') else 0.0
        lower = [t for t, a, b in tail_tokens if t.islower() and len(t) >= 3]
        f[I['romaji']] = (1.0 if all(ROMAJI.match(t) for t in lower) else 0.0) if lower else -1.0
        # --- siblings
        dn = digitnorm(name); base, variant = strip_variant(name)
        f[I['sib8']] = self.c8('dn', dn) if dn != name else -1
        f[I['sibA']] = self.cA('dn', dn) if dn != name else -1
        f[I['base8']] = self.c8('base', base)+(self.c8('name', base) if variant else 0)
        f[I['baseA']] = self.cA('base', base)+(self.cA('name', base) if variant else 0)
        f[I['swap8']] = (self.c8('name', base) > 0)+sum(self.c8('name', base+'_'+v) > 0 for v in ('n', 'y')) if variant else -1
        # --- numbers: is a number that reaches into the last four characters usual after its preceding token?
        f[I['num_tail']] = 0.0; f[I['num_prev_width']] = -1.0; f[I['num_prev_value']] = -1.0
        for position in range(len(toks)-1, 0, -1):
            text, a, b = toks[position]
            if b <= tail_start:
                break
            if text.isdigit():
                previous = toks[position-1][0]
                f[I['num_tail']] = 1.0
                f[I['num_prev_width']] = self.c8('tokd', previous+'\x00'+str(len(text)))
                f[I['num_prev_value']] = self.c8('tok2', previous+'\x00'+text)
                break
        # --- commands in the name
        commands = command_tokens(name)
        f[I['cmd_any']] = 1.0 if commands else 0.0
        f[I['cmd_tail']] = 1.0 if any(end > tail_start for d, b, start, end in commands) else 0.0
        # --- context anchors
        tail_lower = [t.lower() for t, a, b in tail_tokens if len(t) > 1 and not t.isdigit()]
        own_numbers = DIGITS.findall(name); delta = -1.0
        for kind in ('nb', 'sa', 'lg', 'gr'):
            for anchor, anchor_dn, anchor_base, anchor_head, anchor_tokens in ctx._prepared[kind]:
                if anchor_dn == dn and anchor != name and own_numbers:
                    distance = sum(abs(int(x)-int(y)) for x, y in zip(own_numbers, DIGITS.findall(anchor)))
                    if delta < 0 or distance < delta:
                        delta = float(distance)
        f[I['sib_delta']] = delta
        copy_any = 0; explained = 0
        for kind in ('nb', 'sa', 'lg', 'gr'):
            anchors = ctx._prepared[kind]
            f[I[kind+'_any']] = 1.0 if anchors else 0.0
            if not anchors:
                f[I[kind+'_lcp']] = float('nan')
                continue
            same_head = 0; best_lcp = 0; sib = 0; edit = 0; best_suf = 0; append = 0; tailtok = 0
            for anchor, anchor_dn, anchor_base, anchor_head, anchor_tokens in anchors:
                if anchor_head == head and head:
                    same_head = 1
                k = lcp(name, anchor)
                if k > best_lcp:
                    best_lcp = k
                k = lcs(name, anchor)
                if k > best_suf:
                    best_suf = k
                if anchor_dn == dn and anchor != name:
                    sib = 1
                if anchor_base == base and anchor != name:
                    edit = 1
                if len(name) > len(anchor) and name.startswith(anchor):
                    rest = name[len(anchor):]
                    if n-len(anchor) <= 8 and (self.c8('fseg', rest.lstrip('_')) > 0 or self.c8('ftok', rest.lstrip('_')) > 0 or rest.lstrip('_').isdigit()):
                        append = 1
                if tail_lower and all(t in anchor_tokens for t in tail_lower):
                    tailtok = 1
            f[I[kind+'_head']] = same_head
            f[I[kind+'_lcp']] = min(best_lcp, n)-tail_start     # >= 0: the whole pre-tail prefix is shared with an anchor
            f[I[kind+'_sib']] = sib; f[I[kind+'_edit']] = edit; f[I[kind+'_suf']] = best_suf
            f[I[kind+'_append']] = append; f[I[kind+'_tailtok']] = tailtok if tail_lower else -1
            if best_lcp >= tail_start:
                copy_any = 1
            if sib or edit or append:
                explained = 1
        # the search itself proposes prefixes copied from known names, so a copied prefix counts only with its tail
        f[I['copy_any']] = copy_any; f[I['copy_explained']] = 1.0 if (copy_any and explained) else 0.0
        f[I['copy_tail']] = f[I['tail_best']] if (copy_any and not explained) else float('nan')
        f[I['lg_exact']] = 1.0 if ctx.legacy.get(name) else 0.0
        if ctx.anims:
            f[I['anim_tailtok']] = (1.0 if all(t in ctx.anim_tokens for t in tail_lower) else 0.0) if tail_lower else -1.0
            body_tokens = {t.lower() for t in real[1:] if len(t) > 1 and not t.isdigit()}
            f[I['anim_overlap']] = len(body_tokens & ctx.anim_tokens)/len(body_tokens) if body_tokens else -1.0
            lowered = body.lower()
            f[I['anim_lcp']] = max(lcs(lowered, r.lower()) for r in ctx.anim_remainders)
        else:
            f[I['anim_tailtok']] = -2.0; f[I['anim_overlap']] = -2.0; f[I['anim_lcp']] = -1.0
        # --- owner
        key = kb.ncodes_key(len(ctx.codes))
        f[I['ncodes']] = len(ctx.codes)
        total = self.c8('ncodestot', key)
        f[I['head_ncodes']] = self.c8('ncodes', key+'\x00'+head)/total if total else 0.0
        # --- structure
        if ctx.has_structure:
            keys = name_keys(name)
            f[I['st_head']] = self.struct_llr('head='+keys['head'])
            f[I['st_fseg']] = self.struct_llr('fseg='+keys['fseg'])
            f[I['st_cmd']] = self.struct_llr('cmd='+keys['cmd'])
            f[I['st_var']] = self.struct_llr('var='+keys['var'])
            if commands:
                digits, buttons, start, end = commands[-1]
                last = BUTTON_MASK[buttons[-1]]
                f[I['btn_agree']] = (1.0 if last in ctx.buttons else -1.0) if ctx.buttons else 0.0
                first_digits = commands[0][0]
                if ctx.directions and first_digits:
                    f[I['dir_agree']] = 1.0 if any(d == first_digits or (len(d) > 1 and first_digits in d) for d in ctx.directions) else -1.0
            else:
                f[I['btn_agree']] = -2.0 if ctx.buttons else 0.0
            pred_cmd = 0
            for pred, (digits, mask) in ctx.pred.items():
                if mask and len(name) > len(pred) and name.startswith(pred):
                    rest = name[len(pred):].lstrip('_').upper()
                    if rest.endswith(_button_name(mask)) and len(rest) <= len(digits)+4:
                        pred_cmd = 1
            f[I['pred_cmd']] = pred_cmd
        else:
            for key in ('st_head', 'st_fseg', 'st_cmd', 'st_var'):
                f[I[key]] = float('nan')
        return f


# ----------------------------------------------------------------------------
# Reasons
# ----------------------------------------------------------------------------

def explain(name, values, context):
    """Readable facts behind one feature vector (independent of the fitted weights)."""
    v = dict(zip(FEATURES, values)); out = []; segments = name.split('_')
    out.append('last four characters %r: %.1f bits under the T8 name model (%.1f with older games)' % (name[-4:], -v['lm8_tail'], -v['lmA_tail']))
    if v['suf8'] >= 5:
        out.append('ends like %d-character suffix %r of another verified T8 name' % (v['suf8'], name[-int(v['suf8']):]))
    elif v['sufA'] >= 5:
        out.append('ends like %d-character suffix %r of an older-game name' % (v['sufA'], name[-int(v['sufA']):]))
    else:
        out.append('no other known name shares its last five characters')
    if v['tailtok8'] == 0:
        out.append('a token in the last four characters occurs in no other T8 name')
    elif v['tailtok8'] > 0:
        out.append('every token in the last four characters is known (rarest seen in %d T8 names)' % v['tailtok8'])
    if v['tail_seg'] == 1:
        out.append('the unknown token at the end splits into known words')
    elif v['tail_seg'] == 0:
        out.append('the unknown token at the end does not split into known words')
    if v['tshape8'] <= 0:
        out.append('no T8 name ends with this pattern of upper case, lower case and digits (%s)' % classes(name[-5:]))
    if v['copy_any'] and not v['copy_explained']:
        out.append('its start copies a context name but its last four characters match no context name')
    if v['fseg8'] > 0:
        out.append('final segment %r ends %d other T8 names' % (segments[-1], v['fseg8']))
    if v['unknown_tokens'] > 0:
        out.append('%d token(s) seen in no other T8 name' % v['unknown_tokens'])
    if v['sib8'] > 0:
        out.append('%d verified T8 name(s) differ only in digits' % v['sib8'])
    if v['swap8'] > 0:
        out.append('same name with another or no terminal variant is verified')
    if v['sib_delta'] >= 0:
        out.append('its numbers are %d away from a context name that differs only in digits' % v['sib_delta'])
    if v['num_tail'] and v['num_prev_width'] == 0:
        out.append('a number of this width never follows the preceding token in a T8 name')
    if v['head8'] <= 0:
        out.append('head %r is not a known T8 head' % split_head(name)[0])
    elif v['headcode'] < 0.002:
        out.append('head %r is rare for this owner (%.2f%% of its names)' % (split_head(name)[0], 100*v['headcode']))
    for kind, label in (('nb', 'a neighbour label'), ('sa', 'a same-animation move'), ('lg', 'an older-game name in the aligned window'),
                        ('gr', 'a move linked by a cancel')):
        if not v[kind+'_any']:
            continue
        if v[kind+'_sib']:
            out.append('differs only in digits from '+label)
        elif v[kind+'_edit']:
            out.append('differs only in the terminal variant from '+label)
        elif v[kind+'_append']:
            out.append('is '+label+' plus a known ending')
        elif v[kind+'_suf'] >= 5:
            out.append('shares its last %d characters with %s' % (v[kind+'_suf'], label))
        elif v[kind+'_lcp'] >= 0:
            out.append('its first %d characters copy %s, the last four do not' % (len(name)-4, label))
    if v['lg_exact']:
        out.append('older-game name at the exact aligned position')
    if not (v['nb_any'] or v['sa_any'] or v['lg_any'] or v['gr_any']):
        out.append('no visible neighbour, same-animation move, older-game window or linked move')
    if v['btn_agree'] == 1:
        out.append('button in the name agrees with an input command that leads into the move')
    elif v['btn_agree'] == -1:
        out.append('button in the name disagrees with the input commands that lead into the move')
    if v['dir_agree'] == 1:
        out.append('direction digits agree with the input command')
    elif v['dir_agree'] == -1:
        out.append('direction digits disagree with the input command')
    if v['pred_cmd']:
        out.append('equals a move that cancels into it plus the cancel command')
    if not math.isnan(v['st_head']):
        if v['st_head'] <= -6:
            out.append('head class does not fit the move data (%.0f bits)' % v['st_head'])
        elif v['st_head'] >= 3:
            out.append('head class fits the move data (+%.0f bits)' % v['st_head'])
    if not math.isnan(v['st_fseg']) and v['st_fseg'] <= -6:
        out.append('final segment does not fit the move data (%.0f bits)' % v['st_fseg'])
    return out


# ----------------------------------------------------------------------------
# Fitted model (boosted depth-limited trees on binned features) and the judge
# ----------------------------------------------------------------------------

class Model:
    """Additive tree ensemble; fitted by name_judge_train.py, stored as plain JSON."""
    def __init__(self, document):
        self.document = document; self.features = document['features']; self.edges = document['edges']
        self.trees = document['trees']; self.base = document['base']; self.rate = document['learning_rate']
        self.index = [FEATURE_INDEX[f] for f in self.features]
        self.platt = document.get('platt') or dict(a=1.0, b=0.0, log_ratio_shift=0.0)
        self.priors = document.get('prior_in_list') or {}

    def bins(self, values):
        out = []
        for position, i in enumerate(self.index):
            value = values[i]
            out.append(0 if value != value else 1+bisect.bisect_right(self.edges[position], value))
        return out

    def score(self, values, contributions=None):
        """Log-odds that the name is real. contributions (dict) receives the per-feature share."""
        binned = self.bins(values); total = self.base
        for tree in self.trees:
            node = 0
            while True:
                feature, threshold, left, right, value = tree[node]
                if feature < 0:
                    total += self.rate*value
                    break
                nxt = left if binned[feature] <= threshold else right
                if contributions is not None:
                    name = self.features[feature]
                    contributions[name] = contributions.get(name, 0.0)+self.rate*(tree[nxt][4]-value)
                node = nxt
        return total

    def log_ratio(self, score):
        """Calibrated log likelihood ratio: real name against a collision match from a wide search."""
        return self.platt['a']*score+self.platt['b']+self.platt['log_ratio_shift']


def posterior(log_ratios, prior_in_list, size=None):
    """P(name i is the real one | every name of the list).

    Hypotheses: "name i is real, the others are collision matches" for each i, and "none is
    real". prior_in_list is the chance that the search contains the real name at all, spread
    evenly over the list; the likelihood ratios come from the calibrated score.
    """
    if not log_ratios:
        return []
    # size: number of names the search returned for this hash, when some were screened out before scoring
    n = max(len(log_ratios), size or 0); ratios = [math.exp(max(-60.0, min(60.0, r))) for r in log_ratios]
    denominator = (1-prior_in_list)+prior_in_list/n*sum(ratios)
    return [prior_in_list/n*r/denominator for r in ratios]


DEFAULT_PRIOR = 0.1


class Judge:
    def __init__(self, kb, model, thresholds=None):
        self.kb = kb; self.model = model; self.thresholds = thresholds or {}

    @classmethod
    def load(cls, model_path=None, with_overlay=True):
        """Production judge: every verified T8 name is visible, plus every owner-accepted name found in the
        accepted_moves_overlay*.csv files next to the lab folder (read only). The measurements in
        name_judge_findings.md were made with the first overlay only."""
        from name_judge_data import load_all_overlays, load_legacy_names, load_t8_rows as load_rows, load_structure_slim as load_structure
        rows = [r for r in load_rows() if r['game'] == 'T8']
        overlay = load_all_overlays() if with_overlay else {}
        visible = []
        for r in rows:
            name = r['move_name'] if r['move_status'] == 'verified' else overlay.get(r['move_hash'], '')
            visible.append(dict(code=r['code'], id=r['id'], move_name=name, move_hash=r['move_hash'],
                                anim_name=r['anim_name'] if r['anim_status'] == 'verified' else ''))
        kb = KnowledgeBase(visible, load_legacy_names(), load_structure())
        document = json.loads(Path(model_path or _P.shipped('name_judge_model.json')).read_text(encoding='utf-8'))
        return cls(kb, Model(document), document.get('thresholds'))

    def rows_of(self, target):
        key = '0x%08X' % target
        return [(code, identifier) for code, table in self.kb.by_code.items() for identifier, entry in table.items() if entry[2] == key]

    def judge(self, candidates, context, exclusion=None, budget='100000+stems', prior_in_list=None, list_size=None):
        """Grade candidates for one context. Returns dicts sorted best first.

        budget names the kind of list the candidates came from ('100000', '100000+stems',
        '1000000', '1000000+stems'): it selects the decoy-calibrated threshold, which was fixed on
        lists of that size, and the default prior. The probability is conditional on that prior
        (pass prior_in_list to change it) and on list_size, the number of names the search returned
        for this hash when not all of them are passed in.
        """
        scorer = Scorer(self.kb, context, exclusion); rows = []
        for name in dict.fromkeys(candidates):
            values = scorer.features(name); contributions = {}
            rows.append(dict(name=name, score=self.model.score(values, contributions), values=values, contributions=contributions))
        prior = prior_in_list if prior_in_list is not None else self.model.priors.get(str(budget), DEFAULT_PRIOR)
        probabilities = posterior([self.model.log_ratio(r['score']) for r in rows], prior, list_size)
        for row, p in zip(rows, probabilities):
            row['probability'] = p; row['log_likelihood_ratio'] = self.model.log_ratio(row['score'])
        rows.sort(key=lambda r: (-r['score'], r['name']))
        threshold = (self.thresholds or {}).get(str(budget))
        for rank, row in enumerate(rows):
            top = sorted(row['contributions'].items(), key=lambda kv: -abs(kv[1]))[:6]
            row['rank'] = rank+1
            row['margin'] = (row['score']-rows[1]['score'] if len(rows) > 1 else None) if rank == 0 else row['score']-rows[0]['score']
            row['threshold'] = threshold
            row['passes_threshold'] = bool(threshold is not None and rank == 0 and row['score'] > threshold)
            row['reasons'] = explain(row['name'], row['values'], context)
            row['weights'] = ['%s %+.2f' % (k, v) for k, v in top]
        return rows

    def judge_hash(self, target, candidates, budget='100000+stems', prior_in_list=None, list_size=None):
        located = self.rows_of(target)
        if not located:
            raise ValueError('hash 0x%08X is not a move hash of any retail T8 list' % target)
        return self.judge(candidates, Context(self.kb, located), budget=budget, prior_in_list=prior_in_list, list_size=list_size)


def main(argv=None):
    import argparse
    parser = argparse.ArgumentParser(description='Grade same-hash candidate names for one T8 move hash. Accepts nothing.')
    parser.add_argument('--hash', required=True, type=lambda s: int(s, 16))
    parser.add_argument('--file', type=Path, help='one candidate name per line')
    parser.add_argument('--list-type', default='100000+stems', help="kind of search the list came from: '100000', '100000+stems', '1000000', '1000000+stems' (selects threshold and prior)")
    parser.add_argument('--list-size', type=int, help='number of names the search returned for this hash, when only some are given here')
    parser.add_argument('--prior', type=float, help='chance that the list contains the real name at all')
    parser.add_argument('--json', action='store_true')
    parser.add_argument('names', nargs='*')
    args = parser.parse_args(argv)
    names = list(args.names)
    if args.file:
        names += [line.strip() for line in args.file.read_text(encoding='utf-8').splitlines() if line.strip()]
    if not names:
        parser.error('give candidate names or --file')
    from recover import digest
    wrong = [n for n in names if digest(n) != args.hash]
    judge = Judge.load()
    located = judge.rows_of(args.hash); context = Context(judge.kb, located) if located else None
    if context is None:
        parser.error('hash is not a move hash of any retail T8 list')
    verdicts = judge.judge(names, context, budget=args.list_type, prior_in_list=args.prior, list_size=args.list_size)
    if args.json:
        print(json.dumps(dict(hash='0x%08X' % args.hash, context=context.describe(), names_not_hashing_to_target=wrong,
                              verdicts=[{k: v for k, v in r.items() if k not in ('values', 'contributions')} for r in verdicts]), indent=1))
        return
    print('hash 0x%08X  owners %s  rows %s' % (args.hash, ','.join(context.codes[:6]), context.located[:3]))
    print('neighbours', context.neighbors, '| animations', context.anims, '| inputs', sorted({d+_button_name(m) for d, m in context.commands})[:8])
    if wrong:
        print('WARNING: do not hash to the target:', wrong)
    threshold = verdicts[0]['threshold'] if verdicts else None
    print('list type %s: threshold %s (at most one false accept per 1,000 decoy lists of that type)' % (args.list_type, '%.2f' % threshold if threshold is not None else 'unknown'))
    for r in verdicts:
        print('%2d. %-28s score %+7.2f  p %.4f  %s' % (r['rank'], r['name'], r['score'], r['probability'],
                                                      'best name and above the threshold' if r['passes_threshold'] else
                                                      ('above the threshold, not the best name' if threshold is not None and r['score'] > threshold else 'below threshold')))
        for reason in r['reasons']:
            print('      - '+reason)
    print('Nothing is accepted by this tool.')


if __name__ == '__main__':
    main()
