"""Rebuild a native retail T8 PANM with a back-to-front FlatBuffers builder (offline research aid).

    python -I tools/panm_rebuild.py check DIR          # rebuild every *.panm in DIR, report byte-identical count
    python -I tools/panm_rebuild.py rebuild IN OUT [vector]

Native PANMs are standard FlatBuffers builder output: objects are built back to front (the first object built ends
up last in the file), identical vtables are shared, and the raw sample block is appended after the buffer. Tracks are
built in raw-block (skeleton) order, bones in bone-vector order. `vector` builds tracks in bone-vector order instead.
This only reproduces the layout of tracks with raw/static encodings (1, 2); other files are skipped.
"""
import pathlib
import struct
import sys


class Builder:
    def __init__(self):
        self.buf = bytearray()
        self.minalign = 1
        self.vtables = []
        self.fields = None
        self.start = 0

    def size(self):
        return len(self.buf)

    def pad(self, n):
        self.buf[0:0] = bytes(n)

    def prep(self, size, additional):
        self.minalign = max(self.minalign, size)
        self.pad((-(len(self.buf) + additional)) % size)

    def push(self, fmt, value):
        self.buf[0:0] = struct.pack(fmt, value)

    def scalar(self, fmt, value):
        self.prep(struct.calcsize(fmt), 0)
        self.push(fmt, value)

    def refer(self, off):
        self.prep(4, 0)
        return len(self.buf) - off + 4

    def start_table(self):
        self.fields, self.start = {}, len(self.buf)

    def add(self, field, fmt, value, default=0):
        if value == default:
            return
        self.scalar(fmt, value)
        self.fields[field] = len(self.buf)

    def add_offset(self, field, off):
        if not off:
            return
        value = self.refer(off)
        self.push("<I", value)
        self.fields[field] = len(self.buf)

    def end_table(self):
        self.scalar("<i", 0)
        loc = len(self.buf)
        count = max(self.fields) + 1 if self.fields else 0
        vt = [4 + 2 * count, loc - self.start] + [0] * count
        for field, at in self.fields.items():
            vt[2 + field] = loc - at
        raw = struct.pack(f"<{len(vt)}H", *vt)
        use = None
        for existing in self.vtables:
            at = len(self.buf) - existing
            if self.buf[at:at + len(raw)] == raw:
                use = existing
                break
        if use is None:
            self.buf[0:0] = raw
            use = len(self.buf)
            self.vtables.append(use)
        at = len(self.buf) - loc
        struct.pack_into("<i", self.buf, at, use - loc)
        self.fields = None
        return loc

    def string(self, text):
        data = text.encode()
        self.prep(4, len(data) + 1)
        self.buf[0:0] = data + b"\0"
        self.scalar("<I", len(data))
        return len(self.buf)

    def vector(self, offsets):
        self.prep(4, 4 * len(offsets))
        for off in reversed(offsets):
            self.push("<I", self.refer(off))
        self.scalar("<I", len(offsets))
        return len(self.buf)

    def finish(self, root, ident):
        self.prep(self.minalign, 8)
        self.buf[0:0] = ident
        self.push("<I", self.refer(root))
        return bytes(self.buf)


class Reader:
    def __init__(self, d):
        self.d = d

    def u(self, fmt, at):
        return struct.unpack_from(fmt, self.d, at)[0]

    def fields(self, t):
        vt = t - self.u("<i", t)
        n = (self.u("<H", vt) - 4) // 2
        return {i: t + o for i in range(n) if (o := self.u("<H", vt + 4 + 2 * i))}

    def ref(self, at):
        return at + self.u("<I", at)

    def string(self, at):
        return self.d[at + 4:at + 4 + self.u("<I", at)].decode()


def decode(d):
    r = Reader(d)
    root = r.u("<I", 0)
    rf = r.fields(root)
    anim = r.ref(rf[3])
    af = r.fields(anim)
    out = {"root2": r.u("<B", rf[2]) if 2 in rf else 0,
           "anim": {i: r.u("<I", at) for i, at in af.items() if i not in (4, 10)}}
    mode = r.ref(af[10])
    out["mode"] = {i: r.u("<I", at) for i, at in r.fields(mode).items()}
    groups = r.ref(af[4])
    if r.u("<I", groups) != 1:
        raise ValueError("groups")
    group = r.ref(groups + 4)
    gf = r.fields(group)
    out["group1"] = r.u("<I", gf[1]) if 1 in gf else 0
    bones = r.ref(gf[2])
    out["bones"] = []
    for i in range(r.u("<I", bones)):
        bone = r.ref(bones + 4 + 4 * i)
        bf = r.fields(bone)
        tracks = r.ref(bf[2])
        if r.u("<I", tracks) != 1:
            raise ValueError("tracks")
        track = r.ref(tracks + 4)
        tf = r.fields(track)
        options = r.ref(tf[5])
        of = sorted((at, i) for i, at in r.fields(options).items())
        t = {i: r.u("<I", at) for i, at in tf.items() if i not in (1, 5)}
        if t.get(3) not in (1, 2):
            raise ValueError("encoding")
        out["bones"].append({"name": r.string(r.ref(bf[1])), "type": r.string(r.ref(tf[1])), "track": t,
                             "options": [(i, r.u("<B", at)) for at, i in of]})
    base = out["anim"][5]
    out["raw"] = d[base:]
    return out


def build(a, vector_order=False):
    b = Builder()
    order = list(range(len(a["bones"])))
    if not vector_order:
        order.sort(key=lambda i: a["bones"][i]["track"].get(6, 0))
    track_refs = {}
    for i in order:
        bone = a["bones"][i]
        b.start_table()
        for field, value in sorted(bone["options"], reverse=True):
            b.add(field, "<B", value)
        options = b.end_table()
        kind = b.string(bone["type"])
        b.pad(4)  # native files have 4 zero bytes between each track table and its "Transform" string
        b.start_table()
        for field in range(8, 0, -1):
            if field == 5:
                b.add_offset(5, options)
            elif field == 1:
                b.add_offset(1, kind)
            elif field in bone["track"]:
                b.add(field, "<I", bone["track"][field])
        track_refs[i] = b.end_table()
    bone_refs = []
    for i, bone in enumerate(a["bones"]):
        name = b.string(bone["name"])
        tracks = b.vector([track_refs[i]])
        b.start_table()
        b.add_offset(2, tracks)
        b.add_offset(1, name)
        bone_refs.append(b.end_table())
    bones = b.vector(bone_refs)
    b.start_table()
    b.add_offset(2, bones)
    b.add(1, "<I", a["group1"])
    group = b.end_table()
    b.pad(4)  # and between the mode table and the group
    b.start_table()
    for field in sorted(a["mode"], reverse=True):
        b.add(field, "<I", a["mode"][field])
    mode = b.end_table()
    groups = b.vector([group])
    b.start_table()
    b.add_offset(10, mode)
    for field in range(9, 0, -1):
        if field == 4:
            b.add_offset(4, groups)
        elif field in a["anim"]:
            b.add(field, "<I", a["anim"][field] if field != 5 else 0xFFFFFFFF)
    anim = b.end_table()
    b.start_table()
    b.add_offset(3, anim)
    b.add(2, "<B", a["root2"])
    root = b.end_table()
    data = bytearray(b.finish(root, b"PANM"))
    # base (animation field 5) is the buffer size; patch the placeholder
    at = data.find(struct.pack("<I", 0xFFFFFFFF))
    struct.pack_into("<I", data, at, len(data))
    return bytes(data) + a["raw"]


def main():
    if sys.argv[1] == "check":
        same = total = skipped = 0
        for p in sorted(pathlib.Path(sys.argv[2]).glob("*.panm")):
            d = p.read_bytes()
            try:
                a = decode(d)
            except (ValueError, KeyError, struct.error):
                skipped += 1
                continue
            total += 1
            out = build(a)
            if out == d:
                same += 1
            elif total - same <= 3:
                first = next(i for i in range(min(len(out), len(d))) if out[i] != d[i]) if len(out) == len(d) else None
                print("differs", p.name, len(d), len(out), "first diff", first)
        print({"identical": same, "rebuilt": total, "skipped_non_raw": skipped})
    elif sys.argv[1] == "rebuild":
        a = decode(pathlib.Path(sys.argv[2]).read_bytes())
        pathlib.Path(sys.argv[3]).write_bytes(build(a, len(sys.argv) > 4 and sys.argv[4] == "vector"))
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
