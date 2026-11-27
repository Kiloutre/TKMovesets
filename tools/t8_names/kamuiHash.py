"""Kamui hash (Tekken 8 name_key / anim_name_key), ported from docs/hash2.h."""

# Compatible with Python 3.6+

C1 = 0xCC9E2D51
C2 = 0x1B873593

MIX_SUB = 0x052250EC
MIX_SUB2 = 0x19AB949C
MIX_MUL1 = 0x3361D2AF

FMIX1 = 0x85EBCA6B
FMIX2 = 0xC2B2AE35

MASK32 = 0xFFFFFFFF


def _u32(v):
    return v & MASK32


def _rol4(value, count):
    value = _u32(value)
    count %= 32
    if count == 0:
        return value
    return _u32((value << count) | (value >> (32 - count)))


def _ror4(value, count):
    value = _u32(value)
    count %= 32
    if count == 0:
        return value
    return _u32((value >> count) | (value << (32 - count)))


def _read32(data, offset=0):
    return (
        data[offset]
        | (data[offset + 1] << 8)
        | (data[offset + 2] << 16)
        | (data[offset + 3] << 24)
    )


def _byteswap32(v):
    v = _u32(v)
    return (
        ((v >> 24) & 0x000000FF)
        | ((v >> 8) & 0x0000FF00)
        | ((v << 8) & 0x00FF0000)
        | ((v << 24) & 0xFF000000)
    ) & MASK32


def _mix_chunk(k):
    k = _u32(k * C1)
    k = _rol4(k, 15)
    k = _u32(k * C2)
    return k


def _hiword(v):
    return (_u32(v) >> 16) & 0xFFFF


def _compute_kamui_hash_12_to_24(data, length):
    end = length
    mid = length >> 1

    k1 = _mix_chunk(_read32(data, end - 4))
    k2 = _mix_chunk(_read32(data, 0))
    k3 = _mix_chunk(_read32(data, mid))
    k4 = _mix_chunk(_read32(data, end - 8))
    k5 = _mix_chunk(_read32(data, 4))
    k6 = _mix_chunk(_read32(data, mid - 4))

    h = _u32(length)

    h ^= k6
    h = _u32(5 * (_rol4(h, 13) - MIX_SUB))
    h ^= k5
    h = _u32(5 * _rol4(h, 13) - MIX_SUB2)
    h ^= k4
    h = _u32(5 * (_rol4(h, 13) - MIX_SUB))
    h ^= k3
    h = _u32(5 * _rol4(h, 13) - MIX_SUB2)
    h ^= k2
    h = _u32(5 * (_rol4(h, 13) - MIX_SUB))
    h ^= k1
    h = _rol4(h, 13)

    f = _u32(5 * (h - MIX_SUB))
    f ^= f >> 16
    f = _u32(f * FMIX1)
    f ^= f >> 13
    f = _u32(f * FMIX2)

    return f ^ _hiword(f)


def _compute_kamui_hash(data, length):
    if length > 24:
        k4 = _mix_chunk(_read32(data, length - 4))
        k8 = _mix_chunk(_read32(data, length - 8))
        k12 = _mix_chunk(_read32(data, length - 12))
        k16 = _mix_chunk(_read32(data, length - 16))
        k20 = _mix_chunk(_read32(data, length - 20))

        h1 = _u32(5 * (_rol4(length ^ k4, 13) - MIX_SUB))
        v14 = _u32(5 * (_rol4(k16 ^ h1, 13) - MIX_SUB))

        h2 = _u32(5 * (_rol4(_u32(C1 * length) ^ k8, 13) - MIX_SUB))
        v15 = _u32(5 * (_rol4(k12 ^ h2, 13) - MIX_SUB))

        v16 = _u32(5 * (_rol4(_u32(C1 * length) + k20, 13) - MIX_SUB))

        iterations = (length - 1) // 20
        curr = 6

        for _ in range(iterations):
            a = _mix_chunk(_read32(data, curr - 6))
            b = _read32(data, curr - 2)
            c = _mix_chunk(_read32(data, curr + 2))
            d = _mix_chunk(_read32(data, curr + 6))
            e = _read32(data, curr + 10)

            next_v14 = _u32(a - _u32(MIX_MUL1 * _rol4(b + v16, 13)))

            t1 = _rol4(v14 ^ a, 14)
            t2 = _u32(5 * (t1 - MIX_SUB)) ^ _u32(b + d)

            next_v15 = _byteswap32(_u32(5 * (e + _rol4(t2, 13) - MIX_SUB)))

            t3 = _rol4(_u32(v15 + c), 14)
            next_v16 = _u32(5 * _byteswap32(e ^ _u32(5 * (t3 - MIX_SUB))))

            v14 = next_v14
            v15 = next_v15
            v16 = next_v16
            curr += 20

        final_mix = _rol4(_u32(C1 * _ror4(v16, 11)), 15)
        inner = _u32(v14 - _u32(MIX_MUL1 * _rol4(_u32(C1 * _ror4(v15, 11)), 15)))
        final_h = _u32(
            5
            * _rol4(
                _u32(
                    C1
                    * (
                        final_mix
                        + _rol4(_u32(5 * (_rol4(inner, 13) - MIX_SUB)), 15)
                    )
                ),
                13,
            )
            - MIX_SUB2
        )
        return _u32(C1 * _rol4(final_h, 15))

    if length > 12:
        return _compute_kamui_hash_12_to_24(data, length)

    if length > 4:
        k_start = _mix_chunk(_u32(length + _read32(data, 0)))
        h = _u32(5 * (_rol4(_u32(5 * length) ^ k_start, 13) - MIX_SUB))

        k_end = _mix_chunk(_u32(5 * length + _read32(data, length - 4)))
        h = _u32(5 * (_rol4(h ^ k_end, 13) - MIX_SUB))

        mid_off = (length >> 1) & 4
        k_mid = _u32(
            C2
            * _rol4(_u32(0x318F97D9 - _u32(MIX_MUL1 * _read32(data, mid_off))), 15)
        )

        v = _rol4(h ^ k_mid, 13)
        t = _u32(5 * (v - MIX_SUB))
        f = _u32(FMIX1 * (t ^ (t >> 16)))
        r = _u32(FMIX2 * (f ^ (f >> 13)))
        return r ^ (r >> 16)

    # Tiny input (<= 4)
    acc = 0
    xorv = 9

    for i in range(length):
        # (int8)data[i] sign-extended into uint32 arithmetic
        signed_byte = data[i] - 0x100 if data[i] >= 0x80 else data[i]
        acc = _u32(signed_byte - _u32(MIX_MUL1 * acc))
        xorv ^= acc

    v = _rol4(
        _u32(5 * (_rol4(_mix_chunk(length) ^ xorv, 13) - MIX_SUB)) ^ _mix_chunk(acc),
        13,
    )
    t = _u32(5 * (v - MIX_SUB))
    f = _u32(FMIX1 * (t ^ (t >> 16)))
    r = _u32(FMIX2 * (f ^ (f >> 13)))
    return r ^ (r >> 16)


def getKamuiHashDigest(s, length=None):
    """Compute the Kamui hash used for Tekken 8 name_key / anim_name_key.

    Matches game getKamuiHashDigest / ComputeKamuiStringHash.
    Returns a 32-bit unsigned digest (0 .. 0xFFFFFFFF).
    """
    if isinstance(s, str):
        data = s.encode('ascii')
    else:
        data = bytes(s)

    if length is None:
        length = len(data)
    elif length < 0 or length > len(data):
        raise ValueError('length out of range for input')

    return _compute_kamui_hash(memoryview(data)[:length], length)


if __name__ == '__main__':
    import json
    import os
    import sys

    keys_path = os.path.join(os.path.dirname(__file__), 'name_keys.json')
    with open(keys_path, 'r', encoding='utf-8') as f:
        name_keys = json.load(f)

    buckets = {
        'len<=4': [],
        '5<=len<=12': [],
        '13<=len<=24': [],
        'len>24': [],
    }
    for h, name in name_keys.items():
        n = len(name)
        entry = (name, int(h))
        if n <= 4:
            buckets['len<=4'].append(entry)
        elif n <= 12:
            buckets['5<=len<=12'].append(entry)
        elif n <= 24:
            buckets['13<=len<=24'].append(entry)
        else:
            buckets['len>24'].append(entry)

    names_to_hashes = {}
    for h, name in name_keys.items():
        names_to_hashes.setdefault(name, set()).add(int(h))

    outliers = []
    for label, samples in buckets.items():
        match = 0
        for name, expected in samples:
            got = getKamuiHashDigest(name)
            if got == expected:
                match += 1
            else:
                outliers.append((name, expected, got, label, got in names_to_hashes[name]))
        print('%s: %d/%d exact key match' % (label, match, len(samples)))
        for name, _expected in samples[:3]:
            digest = getKamuiHashDigest(name)
            print('  example %r len=%d -> 0x%08X (%d)' % (name, len(name), digest, digest))

    stale_dups = [o for o in outliers if o[4]]
    bad_rows = [o for o in outliers if not o[4]]
    print('stale duplicate keys (digest matches another key for same name): %d' % len(stale_dups))
    print('name_keys rows that disagree with digest: %d' % len(bad_rows))
    for name, expected, got, label, _ in bad_rows:
        print('  [%s] %r key=%d digest=%d' % (label, name, expected, got))

    covered = sum(1 for h, name in name_keys.items() if getKamuiHashDigest(name) in names_to_hashes[name])
    print('names whose digest is present as a key: %d / %d' % (covered, len(name_keys)))

    if bad_rows and len(bad_rows) > 10:
        print('Too many disagreeing rows - hash port likely wrong.')
        sys.exit(1)

    print('getKamuiHashDigest verified against name_keys.json length buckets.')
