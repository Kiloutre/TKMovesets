"""Mutate a native retail T8 PANM one layout aspect at a time (offline research aid for .docs/retail-t8-panm-layout.md).

    python -I tools/panm_mutate.py rawvector NATIVE.panm OUT.panm   # sample blocks in bone-vector order (encoder order)
    python -I tools/panm_mutate.py rawpacked NATIVE.panm OUT.panm   # same order as native, blocks only 4-byte separated

Only the raw sample block and the per-track offset fields (plus the animation extent fields) change, so the result
can be fed to `--headless compile-panm-t8` to see which aspect the game's decoder depends on.
"""
import struct
import sys


def tracks(d):
    u32 = lambda a: struct.unpack_from("<I", d, a)[0]
    i32 = lambda a: struct.unpack_from("<i", d, a)[0]
    u16 = lambda a: struct.unpack_from("<H", d, a)[0]

    def field(t, i):
        vt = t - i32(t)
        if 4 + 2 * i >= u16(vt):
            return 0
        o = u16(vt + 4 + 2 * i)
        return t + o if o else 0
    ref = lambda a: a + u32(a)
    anim = ref(field(u32(0), 3))
    group = ref(ref(field(anim, 4)) + 4)
    bones = ref(field(group, 2))
    out = []
    for i in range(u32(bones)):
        track = ref(ref(field(ref(bones + 4 + 4 * i), 2)) + 4)
        off_field, size_field = field(track, 6), field(track, 7)
        out.append({"off_field": off_field, "off": u32(off_field) if off_field else 0, "size": u32(size_field)})
    return anim, [field(anim, i) for i in (5, 6, 7, 9)], out


def main():
    mode, src, dst = sys.argv[1:4]
    d = bytearray(open(src, "rb").read())
    anim, (base_f, *extent_fields), ts = tracks(d)
    base = struct.unpack_from("<I", d, base_f)[0]
    if any(not t["off_field"] and t["off"] for t in ts) or sum(1 for t in ts if not t["off_field"]) > 1:
        sys.exit("unexpected track layout")
    if mode == "rawvector":
        order = list(range(len(ts)))
        align = 8
    elif mode == "rawpacked":
        order = sorted(range(len(ts)), key=lambda i: ts[i]["off"])
        align = 4
    else:
        sys.exit(__doc__)
    raw = d[base:]
    out = bytearray()
    new_offsets = {}
    for n, i in enumerate(order):
        if n:
            out += bytes(4)
            out += bytes((-len(out)) % align)
        new_offsets[i] = len(out)
        out += raw[ts[i]["off"]:ts[i]["off"] + ts[i]["size"]]
    out += bytes(4)
    out += bytes((-len(out)) % align)
    out += bytes(16)
    zero = [i for i, t in enumerate(ts) if not t["off_field"]]
    if zero and new_offsets[zero[0]] != 0:
        sys.exit("track without offset field would move; mutation needs a vtable change")
    for i, t in enumerate(ts):
        if t["off_field"]:
            struct.pack_into("<I", d, t["off_field"], new_offsets[i])
    for f in extent_fields:
        if f:
            struct.pack_into("<I", d, f, len(out))
    d = d[:base] + out
    open(dst, "wb").write(d)
    print({"bytes": len(d), "extent": len(out)})


if __name__ == "__main__":
    main()
