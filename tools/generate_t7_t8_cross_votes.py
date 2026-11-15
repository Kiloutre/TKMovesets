"""Cross-character requirement/property ID votes for the donor-based T7 -> retail T8 converter.

For every character exported from both games, moves with the same name are aligned the way
T7ToRetailT8::ConvertWithDonor aligns them (cancel requirement lists, hit-condition
requirement lists, extra/start/end property lists). Votes from all characters are pooled.
An ID is emitted only when its votes are unanimous within every character that saw it and
the pooled winner holds >= 90% of at least 5 votes. The converter uses these after its own
donor votes and before the packaged alias catalog.

The same alignment read the other way (retail T8 id -> T7 id, same acceptance rule) is written to
T8ToT7Votes.inc for the T8 -> T7 converter (T8ToT7.cpp).

Run (the exports are LZ4-compressed TKM2 files):
    uv run --with lz4 python -B tools/generate_t7_t8_cross_votes.py [--check]
"""
import argparse, collections, json, pathlib, struct, sys, tempfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
RESEARCH = ROOT / ".docs" / "research"  # local notes, git-ignored
sys.path.insert(0, str(RESEARCH / "t7-to-retail-t8-animation"))
from tkm import T7, T8, khash, decode  # noqa: E402

CHARS = ROOT / ".dev" / "chars"
T7_DIR = CHARS / "t7_tkm_extracted_chars_full_mota"
T8_DIRS = [CHARS / "t8_tkm_extracted_chars_full_panm", CHARS / "t8_tkm_extracted_chars_not_full_mota"]
PAIRS = {  # T7 export name -> retail T8 export name (same character)
    "Alisa": "Alisa Bosconovitch", "Anna": "Anna Williams", "Armor King": "Armor King", "Asuka": "Asuka Kazama",
    "Bob Satsuma": "Bob", "Bryan": "Bryan Fury", "Claudio": "Claudio Serafino", "Devil Jin": "Devil Jin",
    "Dragunov": "Sergei Dragunov", "Eddy": "Eddy Gordo", "Feng (2)": "Feng Wei", "Heihachi": "Heihachi Mishima",
    "Hwoarang": "Hwoarang", "Jack": "Jack-8", "Jin": "Jin Kazama", "Kazuya": "Kazuya Mishima", "King": "King",
    "Kuma": "Kuma", "Kunimitsu": "Kunimitsu", "Lars": "Lars Alexandersson", "Law": "Marshall Law",
    "Lee": "Lee Chaolan", "Lidia": "Lidia Sobieska", "Lin Xiaoyu": "Ling Xiaoyu", "Nina": "Nina Williams",
    "Panda": "Panda", "Paul": "Paul Phoenix", "Shaheen": "Shaheen", "Steve Fox": "Steve Fox",
    "Yoshimitsu": "Yoshimitsu", "Zafina": "Zafina",
}
OUT_INC = ROOT / "TKMovesets2" / "GameSpecific" / "MovesetConverters" / "T7ToRetailT8Votes.inc"
OUT_JSON = RESEARCH / "t7-to-retail-t8-cross-votes.json"
OUT_REVERSE_INC = ROOT / "TKMovesets2" / "GameSpecific" / "MovesetConverters" / "T8ToT7Votes.inc"
NULL = 0xFFFFFFFFFFFFFFFF
u32 = lambda b, o: struct.unpack_from("<I", b, o)[0]
u64 = lambda b, o: struct.unpack_from("<Q", b, o)[0]


def expand(path: pathlib.Path, scratch: pathlib.Path) -> pathlib.Path:
    data = path.read_bytes()
    if data[0x60] == 0:
        return path
    import lz4.block
    start, size = struct.unpack_from("<I", data, 0x50)[0], struct.unpack_from("<Q", data, 0x58)[0]
    if data[0x60] != 1:
        raise SystemExit(f"{path.name}: only LZ4 exports are supported")
    header = bytearray(data[:start]); header[0x60] = 0
    out = scratch / (path.stem + ".raw")
    out.write_bytes(bytes(header) + lz4.block.decompress(data[start:], uncompressed_size=size))
    return out


def command(c):
    if (c >> 32) == 0 and c >= 0x8000:
        return c if c <= 0x8002 else c + 2 if c <= 0x800A else c + 7
    return {0x410: 0x10, 0x440: 0x40}.get(c, c)


def votes(t7: T7, t8: T8):
    names7 = [m[0] for m in t7.moves()]
    by_name = {}
    for row in range(t8.count("moves")):
        r = t8.rec("moves", row); by_name.setdefault(decode(u64(r, 0), u64(r, 8)), row)
    pairs = [(row, by_name[khash(n)]) for row, n in enumerate(names7) if khash(n) in by_name]

    def l7(table, start, end):
        out = []
        if start == NULL: return out
        for row in range(start, min(t7.count(table), start + 4096)):
            out.append(row)
            if end(t7.rec(table, row)): return out
        return []

    def l8(table, pointer, end):
        out, start = [], t8.index_of(table, pointer)
        if start is None or not 0 <= start < t8.count(table): return out
        for row in range(start, min(t8.count(table), start + 4096)):
            out.append(row)
            if end(t8.rec(table, row)): return out
        return []

    req, prop = collections.defaultdict(collections.Counter), collections.defaultdict(collections.Counter)

    def requirements(a7, a8):
        a = l7("requirements", a7, lambda r: u32(r, 0) == 881); b = l8("requirements", a8, lambda r: u32(r, 0) == 1100)
        if a and len(a) == len(b):
            for x, y in zip(a, b):
                ra, rb = t7.rec("requirements", x), t8.rec("requirements", y)
                if u32(ra, 4) == u32(rb, 4): req[u32(ra, 0)][u32(rb, 0)] += 1

    def end7(r):
        q = u64(r, 0); return q == NULL or q >= t7.count("requirements") or u32(t7.rec("requirements", q), 0) == 881

    def end8(r):
        q = t8.index_of("requirements", u64(r, 0)); return q is None or u32(t8.rec("requirements", q), 0) == 1100

    for r7, r8 in pairs:
        m, d = t7.rec("moves", r7), t8.rec("moves", r8)
        c7 = l7("cancels", u64(m, 0x20), lambda r: u64(r, 0) == 0x8000)
        c8 = l8("cancels", u64(d, 0x98), lambda r: u64(r, 0) == 0x8000)
        if c7 and len(c7) == len(c8):
            for x, y in zip(c7, c8):
                a, b = t7.rec("cancels", x), t8.rec("cancels", y)
                if command(u64(a, 0)) == u64(b, 0): requirements(u64(a, 8), u64(b, 8))
        h7, h8 = l7("hit_conditions", u64(m, 0x60), end7), l8("hit_conditions", u64(d, 0x110), end8)
        if h7 and len(h7) == len(h8):
            for x, y in zip(h7, h8): requirements(u64(t7.rec("hit_conditions", x), 0), u64(t8.rec("hit_conditions", y), 0))
        p7 = l7("extra_properties", u64(m, 0x80), lambda r: u32(r, 0) == 0 and u32(r, 4) == 0)
        p8 = l8("extra_properties", u64(d, 0x138), lambda r: u32(r, 0) == 0 and u32(r, 16) == 0)
        if p7 and len(p7) == len(p8):
            for x, y in zip(p7, p8):
                a, b = t7.rec("extra_properties", x), t8.rec("extra_properties", y)
                if u32(a, 0) == u32(b, 0) and u32(a, 8) == u32(b, 20): prop[u32(a, 4)][u32(b, 16)] += 1
        for table, f7, f8 in (("start_properties", 0x88, 0x140), ("end_properties", 0x90, 0x148)):
            a = l7(table, u64(m, f7), lambda r: u32(r, 8) == 881); b = l8(table, u64(d, f8), lambda r: u32(r, 8) == 1100)
            if a and len(a) == len(b):
                for x, y in list(zip(a, b))[:-1]:
                    ra, rb = t7.rec(table, x), t8.rec(table, y)
                    if u32(ra, 12) == u32(rb, 12): prop[u32(ra, 8)][u32(rb, 8)] += 1
                    requirements(u64(ra, 0), u64(rb, 0))
    return len(pairs), req, prop


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="verify the generated files are current")
    args = parser.parse_args()
    pooled = {"req": collections.defaultdict(collections.Counter), "prop": collections.defaultdict(collections.Counter)}
    winners = {"req": collections.defaultdict(set), "prop": collections.defaultdict(set)}
    # Reverse direction (retail T8 id -> T7 id), same pairs.
    rpooled = {"req": collections.defaultdict(collections.Counter), "prop": collections.defaultdict(collections.Counter)}
    rwinners = {"req": collections.defaultdict(set), "prop": collections.defaultdict(set)}
    characters = []
    with tempfile.TemporaryDirectory() as scratch:
        for t7name, t8name in sorted(PAIRS.items()):
            source = T7_DIR / f"T7_{t7name}.tkmvst"
            target = next((d / f"T8_{t8name}.tkmvst" for d in T8_DIRS if (d / f"T8_{t8name}.tkmvst").exists()), None)
            if not source.exists() or target is None:
                continue
            t7 = T7(str(expand(source, pathlib.Path(scratch))))
            raw8 = expand(target, pathlib.Path(scratch))
            n, req, prop = votes(t7, T8(str(raw8)))
            if raw8 != target: raw8.unlink()
            characters.append({"t7": t7name, "t8": t8name, "paired_moves": n})
            for kind, local in (("req", req), ("prop", prop)):
                reverse = collections.defaultdict(collections.Counter)
                for source_id, targets in local.items():
                    pooled[kind][source_id].update(targets)
                    winners[kind][source_id].add(max(targets, key=targets.get))
                    for target_id, n in targets.items(): reverse[target_id][source_id] += n
                for target_id, sources in reverse.items():
                    rpooled[kind][target_id].update(sources)
                    rwinners[kind][target_id].add(max(sources, key=sources.get))
            print(f"{t7name:12} -> {t8name:20} paired {n}", file=sys.stderr)
    rows, evidence = [], {"characters": characters, "requirements": {}, "properties": {}}
    for kind, is_property in (("req", False), ("prop", True)):
        for source_id, targets in sorted(pooled[kind].items()):
            total = sum(targets.values()); best = max(targets, key=targets.get)
            accepted = len(winners[kind][source_id]) == 1 and total >= 5 and targets[best] * 10 >= total * 9
            evidence["properties" if is_property else "requirements"][str(source_id)] = {
                "targets": {str(k): v for k, v in sorted(targets.items())}, "accepted": accepted,
                "character_winners": sorted(winners[kind][source_id])}
            if accepted: rows.append((is_property, source_id, best, targets[best], total))
    reverse_rows = []
    for kind, is_property in (("req", False), ("prop", True)):
        for source_id, targets in sorted(rpooled[kind].items()):
            total = sum(targets.values()); best = max(targets, key=targets.get)
            if len(rwinners[kind][source_id]) == 1 and total >= 5 and targets[best] * 10 >= total * 9:
                reverse_rows.append((is_property, source_id, best, targets[best], total))
    reverse_inc = ["// Generated by tools/generate_t7_t8_cross_votes.py: pooled same-name move alignment over",
                   f"// {len(characters)} characters exported from both T7 and retail T8, read from T8 to T7. {{property, t8 id, t7 id, votes, total}}"]
    reverse_inc += [f"    {{{'true' if p else 'false'}, {s}u, {t}u, {v}u, {n}u}}," for p, s, t, v, n in sorted(reverse_rows)]
    reverse_text = "\n".join(reverse_inc) + "\n"
    inc = ["// Generated by tools/generate_t7_t8_cross_votes.py: pooled same-name move alignment over",
           f"// {len(characters)} characters exported from both T7 and retail T8. {{property, t7 id, t8 id, votes, total}}"]
    inc += [f"    {{{'true' if p else 'false'}, {s}u, {t}u, {v}u, {n}u}}," for p, s, t, v, n in sorted(rows)]
    inc_text = "\n".join(inc) + "\n"
    json_text = json.dumps(evidence, indent=1, sort_keys=True) + "\n"
    if args.check:
        stale = [p for p, text in ((OUT_INC, inc_text), (OUT_JSON, json_text), (OUT_REVERSE_INC, reverse_text)) if not p.exists() or p.read_text() != text]
        if stale: raise SystemExit("stale: " + ", ".join(map(str, stale)))
        return
    OUT_INC.write_text(inc_text, newline="\n"); OUT_JSON.write_text(json_text, newline="\n")
    OUT_REVERSE_INC.write_text(reverse_text, newline="\n")
    print(f"{len(rows)} accepted IDs ({len(reverse_rows)} reverse) from {len(characters)} characters", file=sys.stderr)


if __name__ == "__main__":
    main()
