"""Training-only length prior; one global prefix budget across lengths13..24.

This cannot infer a length from a hash. It searches all supported branches with
smoothed training frequencies and labels the unsupported length range openly.
"""
import collections as co
import math


class LengthPrior:
    def __init__(self, rows, names, field):
        self.global_counts = co.Counter(); self.owner_counts = co.defaultdict(co.Counter)
        global_seen = set(); owner_seen = set()
        for row in rows:
            name = row[field]; code = row['code']
            if name not in names or not name:
                continue
            if name not in global_seen:
                self.global_counts[len(name)] += 1; global_seen.add(name)
            if (code, name) not in owner_seen:
                self.owner_counts[code][len(name)] += 1; owner_seen.add((code, name))

    def weights(self, codes, lengths=range(13,25)):
        lengths = tuple(sorted(set(lengths))); size = len(lengths)
        if not size:
            raise ValueError('At least one searched length required')
        def distribution(counter):
            total = sum(counter[n] for n in lengths)
            return {n: (counter[n] + 1) / (total + size) for n in lengths}
        base = distribution(self.global_counts)
        local = [distribution(self.owner_counts[c]) for c in sorted(set(codes)) if self.owner_counts[c]]
        if not local:
            return base
        return {n: .05*base[n] + .95*sum(d[n] for d in local)/len(local) for n in lengths}

    def allocate(self, budget, codes, lengths=range(13,25)):
        if not isinstance(budget, int) or isinstance(budget, bool) or budget < 0:
            raise ValueError('Budget must be a nonnegative integer')
        weights = self.weights(codes, lengths)
        out = {n: math.floor(budget * w) for n, w in weights.items()}
        remaining = budget - sum(out.values())
        for n in sorted(weights, key=lambda n: (-(budget*weights[n]-out[n]), n))[:remaining]:
            out[n] += 1
        assert sum(out.values()) == budget
        return out

    def metadata(self, codes):
        total = sum(self.global_counts.values())
        return dict(weights=self.weights(codes), supported_lengths=[13,24],
                    training_names=total,
                    training_names_outside_supported_lengths=sum(c for n,c in self.global_counts.items() if not 13<=n<=24),
                    policy='Add-one length smoothing,95% owner distribution plus5% global; largest remainders allocate ONE total budget.',
                    limitation='Only lengths13..24 are covered by prefix inversion; finite-name lanes handle other lengths.')
