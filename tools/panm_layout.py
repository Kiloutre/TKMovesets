"""Structural dump/diff of retail T8 PANM FlatBuffers layouts (offline research aid).

    python -I tools/panm_layout.py dump FILE.panm
    python -I tools/panm_layout.py diff NATIVE.panm OTHER.panm

The game's decoder refuses PANMs whose layout differs from native files even when the data is valid.
This walks the known schema (root -> animation -> bone groups -> bones -> tracks) and reports, per object,
its address, vtable address, vtable contents, field values and the order objects appear in the buffer,
so the independent encoder (RetailPanm::Encode, `--headless panm-reencode`) can be compared with native files.
"""
import struct
import sys


class Buf:
    def __init__(self, data):
        self.b = data
        self.objects = []  # (address, kind, detail)

    def u16(self, at): return struct.unpack_from("<H", self.b, at)[0]
    def u32(self, at): return struct.unpack_from("<I", self.b, at)[0]
    def i32(self, at): return struct.unpack_from("<i", self.b, at)[0]

    def table(self, at, kind):
        vt = at - self.i32(at)
        size, obj = self.u16(vt), self.u16(vt + 2)
        fields = [self.u16(vt + 4 + 2 * i) for i in range((size - 4) // 2)]
        self.objects.append((at, kind, {"vtable": vt, "vtable_rel": vt - at, "vtable_size": size, "object_size": obj, "fields": fields}))
        return fields

    def field(self, at, index):
        vt = at - self.i32(at)
        size = self.u16(vt)
        if 4 + 2 * index >= size:
            return 0
        off = self.u16(vt + 4 + 2 * index)
        return at + off if off else 0

    def ref(self, at):
        return at + self.u32(at) if at else 0

    def vector(self, at, kind):
        n = self.u32(at)
        self.objects.append((at, kind, {"count": n}))
        return [at + 4 + 4 * i + self.u32(at + 4 + 4 * i) for i in range(n)]

    def string(self, at, kind):
        n = self.u32(at)
        text = self.b[at + 4:at + 4 + n].decode("utf-8", "replace")
        self.objects.append((at, kind, {"len": n, "text": text}))
        return text


def walk(data):
    f = Buf(data)
    root = f.u32(0)
    rf = f.table(root, "root")
    values = {"root_fields": [(i, f.u32(f.field(root, i))) if f.field(root, i) else (i, None) for i in range(len(rf))]}
    anim = f.ref(f.field(root, 3))
    af = f.table(anim, "animation")
    values["animation_fields"] = [(i, f.u32(f.field(anim, i)) if f.field(anim, i) else None) for i in range(len(af))]
    for i in range(len(af)):
        at = f.field(anim, i)
        if at and i not in (1, 2, 3, 4, 5, 6, 7, 9):  # unknown table/vector-valued fields
            target = f.ref(at)
            if 0 < target < len(data) - 4:
                try:
                    f.table(target, f"animation.field{i}")
                except struct.error:
                    pass
    groups = f.vector(f.ref(f.field(anim, 4)), "groups")
    for g in groups:
        f.table(g, "group")
        for bone in f.vector(f.ref(f.field(g, 2)), "bones"):
            f.table(bone, "bone")
            name = f.string(f.ref(f.field(bone, 1)), "bone.name")
            for track in f.vector(f.ref(f.field(bone, 2)), "tracks"):
                tf = f.table(track, f"track[{name}]")
                f.string(f.ref(f.field(track, 1)), "track.type")
    f.objects.sort()
    return f, values


def kinds(objects):
    out = []
    for at, kind, detail in objects:
        k = kind.split("[")[0]
        if not out or out[-1][0] != k:
            out.append([k, 1])
        else:
            out[-1][1] += 1
    return out


def dump(path):
    data = open(path, "rb").read()
    f, values = walk(data)
    print(f"{path}: {len(data)} bytes, {len(f.objects)} objects")
    print("root fields", values["root_fields"])
    print("animation fields", values["animation_fields"])
    print("object order:", " ".join(f"{k}x{n}" for k, n in kinds(f.objects)))
    seen = {}
    for at, kind, d in f.objects:
        k = kind.split("[")[0]
        if "vtable" in d and k not in seen:
            seen[k] = d
            print(f"  {k:18} vtable_rel={d['vtable_rel']:+} vtable_size={d['vtable_size']} object_size={d['object_size']} fields={d['fields']}")
    vts = {d["vtable"] for _, _, d in f.objects if "vtable" in d}
    print("distinct vtables:", len(vts), "tables:", sum(1 for _, _, d in f.objects if "vtable" in d))
    return f, values


def diff(native, other):
    a, va = dump(native)
    print()
    b, vb = dump(other)
    print("\nDifferences:")
    sa = {k.split("[")[0]: d for _, k, d in a.objects if "vtable" in d}
    sb = {k.split("[")[0]: d for _, k, d in b.objects if "vtable" in d}
    for k in sorted(set(sa) | set(sb)):
        x, y = sa.get(k), sb.get(k)
        if not x or not y:
            print(f"  {k}: only in {'native' if x else 'other'}")
            continue
        for key in ("vtable_rel", "vtable_size", "object_size", "fields"):
            if (x[key] > 0) != (y[key] > 0) if key == "vtable_rel" else x[key] != y[key]:
                print(f"  {k}.{key}: native={x[key]} other={y[key]}")
    if kinds(a.objects) != kinds(b.objects):
        print("  object order differs")
    na = len({d["vtable"] for _, _, d in a.objects if "vtable" in d})
    nb = len({d["vtable"] for _, _, d in b.objects if "vtable" in d})
    if na != nb:
        print(f"  vtable sharing: native {na} distinct, other {nb} distinct")
    for (i, x), (_, y) in zip(va["animation_fields"], vb["animation_fields"]):
        if x != y:
            print(f"  animation field {i}: native={x} other={y}")
    if va["root_fields"] != vb["root_fields"]:
        print(f"  root fields: native={va['root_fields']} other={vb['root_fields']}")


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "dump":
        dump(sys.argv[2])
    elif len(sys.argv) == 4 and sys.argv[1] == "diff":
        diff(sys.argv[2], sys.argv[3])
    else:
        sys.exit(__doc__)
