"""Typed local T7/T8 names with explicit, namespace-specific training masks.

Build with build_name_corpora.py. T7 "unflagged" is a heuristic review policy,
not a claim that the extracts are official or unmodified. Raw rows are audit
data; use training_view() before fitting a held-out experiment.
"""
import t8names_paths as _P
from dataclasses import dataclass
from pathlib import Path
import gzip
import json
import re

HERE = _P.LAB
DEFAULT_DIRECTORY = HERE / 'typed_corpus'
NAMESPACES = ('move', 'animation')
FIELDS = {'move': ('move_name', 'move_hash', 'move_status'),
          'animation': ('anim_name', 'anim_hash', 'anim_status')}
PLACEHOLDER = re.compile(r'^(?:nk|ak)[0-9a-fA-F]{8}_*$')


def is_label(value):
    """Only exact exported hash markers are placeholders; akiresu is valid."""
    return isinstance(value, str) and bool(value) and not PLACEHOLDER.fullmatch(value)


def default_family(name, namespace):
    """Conservative default; experiments can supply their own frozen family key."""
    if namespace == 'animation':
        parts = name.split('_', 2)
        if len(parts) == 3:
            text = re.sub(r'(?:_(?:c|f|y|n|l|r|end|start|loop))+$', '', parts[2].lower())
            return parts[1].lower() + '_' + re.sub(r'\d+', '#', text)
        return name.lower()
    return re.sub(r'\d+', '#', name.split('_', 1)[-1].lower())


@dataclass
class TrainingView:
    namespace: str
    names: frozenset
    t8_names: frozenset
    t7_names: frozenset
    rows: list
    stats: dict

    @property
    def t7_rows(self):
        return [r for r in self.rows if r['game'] == 'T7']

    @property
    def t8_rows(self):
        return [r for r in self.rows if r['game'] == 'T8']


class Corpus:
    def __init__(self, directory=DEFAULT_DIRECTORY):
        self.directory = Path(directory)
        self.manifest = json.loads((self.directory / 'manifest.json').read_text(encoding='utf-8'))
        source_rows = json.loads((self.directory / 'sources.json').read_text(encoding='utf-8'))
        self.sources = {r['source_id']: r for r in source_rows}
        self.labels = json.loads((self.directory / 'labels.json').read_text(encoding='utf-8'))
        with gzip.open(self.directory / 'rows.jsonl.gz', 'rt', encoding='utf-8') as f:
            self.rows = [json.loads(line) for line in f]
        self.t7_rows = [r for r in self.rows if r['game'] == 'T7']
        self.t8_rows = [r for r in self.rows if r['game'] == 'T8']

    def targets(self, namespace):
        if namespace not in NAMESPACES:
            raise ValueError('namespace must be move or animation')
        return json.loads((self.directory / f'unresolved_{namespace}_targets.json').read_text(encoding='utf-8'))

    def training_view(self, namespace, *, hidden_names=None, hidden_families=None,
                      family_key=None, t7_policy='none', t7_overlap='mask'):
        """Return safe rows; excluded labels in each namespace are independently blanked.

        hidden_names/hidden_families are mappings keyed by 'move'/'animation'.
        family_key is a one-argument function or such a mapping of functions.
        Omitted family_key uses default_family. Hidden-name families are masked
        automatically, in addition to explicit hidden_families. T8 same-hash
        aliases are excluded globally within their own namespace. The other
        namespace remains usable unless separately excluded.

        T7 'tag' deliberately permits held-out copies and labels every affected
        row. It is a transfer/leakage ablation, not a strict evaluation mode.
        Hashes and row provenance are retained, but no hidden raw-label copy is
        included in returned rows. Caller-provided family functions must be
        frozen before evaluating retention.
        """
        if namespace not in NAMESPACES:
            raise ValueError('namespace must be move or animation')
        if t7_policy not in ('none', 'unflagged', 'all'):
            raise ValueError('t7_policy must be none, unflagged or all')
        if t7_overlap not in ('mask', 'tag'):
            raise ValueError('t7_overlap must be mask or tag')
        hidden_names = hidden_names or {}
        hidden_families = hidden_families or {}
        if set(hidden_names) - set(NAMESPACES) or set(hidden_families) - set(NAMESPACES):
            raise ValueError('unknown namespace in exclusion masks')

        def key(name, ns):
            fn = family_key.get(ns) if isinstance(family_key, dict) else family_key
            return fn(name) if fn else default_family(name, ns)

        literal = {ns: set(hidden_names.get(ns, ())) for ns in NAMESPACES}
        families = {ns: set(hidden_families.get(ns, ())) | {key(n, ns) for n in literal[ns]}
                    for ns in NAMESPACES}
        hashes = {ns: set() for ns in NAMESPACES}
        for ns in NAMESPACES:
            valid = [(r['name'], key(r['name'], ns), set(r['source_hashes']))
                     for r in self.labels if r['game'] == 'T8' and r['namespace'] == ns
                     and r['status'] == 'verified']
            # Close the exclusion relation over families and same-hash aliases.
            # A colliding alias can belong to another family, whose other labels
            # must not reintroduce the now-hidden hash through a second alias.
            while True:
                before = (len(literal[ns]), len(families[ns]), len(hashes[ns]))
                for name, family, source_hashes in valid:
                    if name in literal[ns] or family in families[ns] or source_hashes & hashes[ns]:
                        literal[ns].add(name)
                        families[ns].add(family)
                        hashes[ns].update(source_hashes)
                if before == (len(literal[ns]), len(families[ns]), len(hashes[ns])):
                    break

        stats = dict(namespace=namespace, t7_policy=t7_policy, t7_overlap=t7_overlap,
                     strict=t7_overlap == 'mask', input_rows=len(self.rows), output_rows=0,
                     t7_rows_policy_excluded=0, masked_labels={ns: 0 for ns in NAMESPACES},
                     invalid_labels={ns: 0 for ns in NAMESPACES},
                     t7_overlap_labels={ns: 0 for ns in NAMESPACES},
                     hidden_name_counts={ns: len(literal[ns]) for ns in NAMESPACES},
                     hidden_family_counts={ns: len(families[ns]) for ns in NAMESPACES},
                     hidden_t8_hash_counts={ns: len(hashes[ns]) for ns in NAMESPACES})
        rows = []
        sets = {'T7': set(), 'T8': set()}
        selected_field = FIELDS[namespace][0]
        for raw in self.rows:
            game = raw['game']
            if game == 'T7' and (t7_policy == 'none' or (t7_policy == 'unflagged' and raw['source_flags'])):
                stats['t7_rows_policy_excluded'] += 1
                continue
            row = dict(raw)
            masked = []
            overlaps = []
            for ns, (field, hashfield, statusfield) in FIELDS.items():
                name = row[field]
                valid_status = 'verified' if game == 'T8' else 'source_unverified'
                if row[statusfield] != valid_status:
                    row[field] = ''
                    stats['invalid_labels'][ns] += 1
                    continue
                exact = name in literal[ns]
                related = key(name, ns) in families[ns]
                same_hash = game == 'T8' and row[hashfield] in hashes[ns]
                if exact or related or same_hash:
                    if game == 'T7':
                        stats['t7_overlap_labels'][ns] += 1
                        overlaps.append(ns + (':exact' if exact else ':family'))
                    if game == 'T8' or t7_overlap == 'mask':
                        row[field] = ''
                        masked.append(ns)
                        stats['masked_labels'][ns] += 1
            row['masked_namespaces'] = masked
            row['transfer_overlap'] = overlaps
            if row['move_name'] or row['anim_name']:
                rows.append(row)
                if row[selected_field]:
                    sets[game].add(row[selected_field])
        stats['output_rows'] = len(rows)
        stats['unique_names'] = len(sets['T7'] | sets['T8'])
        stats['t7_unique_names'] = len(sets['T7'])
        stats['t8_unique_names'] = len(sets['T8'])
        return TrainingView(namespace, frozenset(sets['T7'] | sets['T8']),
                            frozenset(sets['T8']), frozenset(sets['T7']), rows, stats)


def load_corpus(directory=DEFAULT_DIRECTORY):
    return Corpus(directory)
