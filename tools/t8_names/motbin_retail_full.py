"""Full retail Tekken 8 .motbin parser: every table, every move with its lists resolved -> JSON.

Record layouts: LukzB TekkenMovesetExtractor `t8_offsetTable` / `t8StructSizes` (in-memory, retail),
which the on-disk file follows with two differences (docs/formats/tkdata.md section 3):
pointers are u64 *indexes* into the target table (0xFFFFFFFFFFFFFFFF = null), and the six
encrypted move fields are 0x20 blocks (motbin_retail.decode_block).
List rules (checked against the live dump, docs/formats/tkdata.md section 5): requirements end at
id 1100, cancels at command 0x8000, extra properties at id 0, start/end properties at id 1100
(retail_moveset_dump.py used 0 there and overran), a move's hit-condition list at the entry whose
requirement list starts with 1100. Cancel `option` is 0 on disk: the game fills it at load.

Usage:
    python motbin_retail_full.py DIR_OR_FILES... [--names DICT.json ...] [--out DIR]
        [--check-live work/movevm/port/retail_live_movesets.json]
Writes OUT/<code>.json: {"header", "tables" (every record), "moves" (resolved)}.
"""
import argparse
import glob
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
import motbin_retail as R  # noqa: E402

NULL = 0xFFFFFFFFFFFFFFFF
REQ_END = 1100
CANCEL_END = 0x8000
SIZES = {"reactions": 0x70, "requirements": 0x14, "hit_conditions": 0x18, "projectiles": 0xE0,
         "pushbacks": 0x10, "pushback_extradata": 0x2, "cancels": 0x28, "group_cancels": 0x28,
         "cancel_extradata": 0x4, "extra_move_props": 0x28, "move_start_props": 0x20,
         "move_end_props": 0x20, "moves": 0x448, "voiceclips": 0xC, "input_sequences": 0x10,
         "inputs": 0x8, "move_key_list": 0x4, "cameras": 0xC, "throw_cameras": 0x10, "dialogues": 0x18}
REACTION_FIELDS = ["front_direction", "back_direction", "left_side_direction", "right_side_direction",
                   "front_counterhit_direction", "downed_direction", "front_rotation", "back_rotation",
                   "left_side_rotation", "right_side_rotation", "vertical_pushback", "downed_rotation",
                   "standing", "crouch", "ch", "crouch_ch", "left_side", "left_side_crouch", "right_side",
                   "right_side_crouch", "back", "back_crouch", "block", "crouch_block", "wallslump",
                   "downed", "_0x6C", "_0x6E"]


def ix(v):
    return None if v == NULL else v


def u(fmt, b, o):
    return struct.unpack_from("<" + fmt, b, o)


def rec_requirements(b, o):
    r, p1, p2, p3, p4 = u("5I", b, o)
    return {"req": r, "params": [p1, p2, p3, p4]}


def rec_cancel(b, o):
    cmd, rq, ex, w0, w1, sf, mv, opt = u("QQQIIIHH", b, o)
    return {"command": "0x%016X" % cmd, "requirements": ix(rq), "extradata": ix(ex),
            "window": [w0, w1], "starting_frame": sf, "move": mv, "option": opt}


def rec_hit_condition(b, o):
    rq, dmg, _pad, rl = u("QIIQ", b, o)
    return {"requirements": ix(rq), "damage": dmg, "reactions": ix(rl)}


def rec_reaction(b, o):
    d = {"pushbacks": [ix(x) for x in u("7Q", b, o)]}
    d.update(zip(REACTION_FIELDS, u("28H", b, o + 0x38)))
    return d


def rec_pushback(b, o):
    v1, v2, v3, ex = u("HHIQ", b, o)
    return {"val1": v1, "val2": v2, "val3": v3, "extradata": ix(ex)}


def rec_extra_prop(b, o):
    t, _p, rq, pid, p1, p2, p3, p4 = u("IIQIIIII", b, o)
    return {"frame": t, "requirements": ix(rq), "id": pid, "params": [p1, p2, p3, p4]}


def rec_other_prop(b, o):
    rq, pid, p1, p2, p3, p4, _p = u("QIIIIII", b, o)
    return {"requirements": ix(rq), "id": pid, "params": [p1, p2, p3, p4]}


def rec_projectile(b, o):
    return {"u1": list(u("36I", b, o)), "hit_condition": ix(u("Q", b, o + 0x90)[0]),
            "cancel": ix(u("Q", b, o + 0x98)[0]), "u2": list(u("16I", b, o + 0xA0))}


def rec_input_sequence(b, o):
    a, c, e, ex = u("HHIQ", b, o)
    return {"u1": a, "u2": c, "u3": e, "extradata": ix(ex)}


def rec_camera(b, o):
    p, ct, l, r, rot = u("IHHHH", b, o)
    return {"pick_probability": p, "camera_type": ct, "left_side_camera_data": l,
            "right_side_camera_data": r, "additional_rotation": rot}


def rec_throw(b, o):
    side, _p, ex = u("IIQ", b, o)
    return {"side": side, "throw_extra": ix(ex)}


def rec_dialogue(b, o):
    t, i, _p, rq, vk, fa = u("HHIQII", b, o)
    return {"type": t, "id": i, "requirements": ix(rq), "voiceclip_key": vk, "facial_anim_idx": fa}


def rec_move(b, o):
    m = {}
    for f, fo in R.FIELDS.items():
        m[f] = R.decode_block(b, o + fo)[0]
    ch = m.pop("chara")
    m["chara_id"] = None if ch is None else ch >> 16
    m["anim_key"], m["skeleton_id"] = u("II", b, o + 0x50)
    m["cancel"] = ix(u("Q", b, o + 0x98)[0])
    m["transition"], m["end_rotation"] = u("Hh", b, o + 0xCC)
    m["hit_condition"] = ix(u("Q", b, o + 0x110)[0])
    m["damage_override"] = u("H", b, o + 0x118)[0]
    (m["anim_max_len_adjuster"], m["anim_max_len"], m["airborne_start"], m["airborne_end"],
     m["ground_fall"]) = u("5I", b, o + 0x11C)
    m["voiceclip"], m["extra_props"], m["start_props"], m["end_props"] = [ix(x) for x in u("4Q", b, o + 0x130)]
    m["u15"], m["_0x154"], m["startup"], m["recovery"] = u("4I", b, o + 0x150)
    m["hitboxes"] = []
    for k in range(8):
        hs, hr, loc = u("3I", b, o + 0x160 + 0x30 * k)
        m["hitboxes"].append({"startup": hs, "recovery": hr, "location": "0x%X" % loc,
                              "floats": [round(x, 4) for x in u("9f", b, o + 0x16C + 0x30 * k)]})
    m["u16"], m["u17"] = u("HH", b, o + 0x2E0)
    m["unk5"] = list(u("88I", b, o + 0x2E4))
    return m


DECODERS = {"reactions": rec_reaction, "requirements": rec_requirements, "hit_conditions": rec_hit_condition,
            "projectiles": rec_projectile, "pushbacks": rec_pushback,
            "pushback_extradata": lambda b, o: u("H", b, o)[0], "cancels": rec_cancel, "group_cancels": rec_cancel,
            "cancel_extradata": lambda b, o: u("I", b, o)[0], "extra_move_props": rec_extra_prop,
            "move_start_props": rec_other_prop, "move_end_props": rec_other_prop, "moves": rec_move,
            "voiceclips": lambda b, o: list(u("3I", b, o)), "input_sequences": rec_input_sequence,
            "inputs": lambda b, o: list(u("II", b, o)), "move_key_list": lambda b, o: u("I", b, o)[0],
            "cameras": rec_camera, "throw_cameras": rec_throw, "dialogues": rec_dialogue}


def parse(d):
    if d[8:11] != b"TEK":
        raise ValueError("not a TEK motbin")
    tabs = R.tables(d)
    end = R.DATA_BASE + max(o + n * SIZES[k] for k, (o, n) in tabs.items())
    if end != len(d):
        raise ValueError("tables end at 0x%X, file is 0x%X" % (end, len(d)))
    out = {}
    for k, (o, n) in tabs.items():
        out[k] = [DECODERS[k](d, R.DATA_BASE + o + i * SIZES[k]) for i in range(n)]
    hdr = {"compile_date": u("I", d, 4)[0], "signature": d[8:11].decode(),
           "original_aliases": list(u("60H", d, 0x30)), "current_aliases": list(u("60H", d, 0xA8)),
           "unknown_aliases": list(u("36H", d, 0x120)),
           "tables": {k: {"offset": "0x%X" % o, "count": n, "record_size": "0x%X" % SIZES[k]}
                      for k, (o, n) in tabs.items()}}
    return hdr, out


class Resolver:
    """Walks the index lists of one parsed motbin; counts every out-of-range / unterminated list."""

    def __init__(self, t):
        self.t = t
        self.errors = {}

    def err(self, k):
        self.errors[k] = self.errors.get(k, 0) + 1

    def reqs(self, i):
        if i is None:
            return None
        rq, out = self.t["requirements"], []
        while i < len(rq):
            r = rq[i]
            out.append([r["req"]] + r["params"])
            if r["req"] == REQ_END:
                return out
            i += 1
        self.err("requirements unterminated")
        return out

    def cancels(self, i, table="cancels"):
        if i is None:
            return []
        cs, out = self.t[table], []
        while i < len(cs):
            c = dict(cs[i])
            c["requirements"] = self.reqs(c["requirements"])
            if c["extradata"] is not None:
                ce = self.t["cancel_extradata"]
                if c["extradata"] < len(ce):
                    c["extradata_value"] = ce[c["extradata"]]
                else:
                    self.err("cancel extradata out of range")
            out.append(c)
            if int(c["command"], 16) == CANCEL_END:
                return out
            i += 1
        self.err(table + " unterminated")
        return out

    def hit_conditions(self, i):
        if i is None:
            return []
        hc, out = self.t["hit_conditions"], []
        while i < len(hc):
            h = hc[i]
            r = self.reqs(h["requirements"])
            if h["reactions"] is not None and h["reactions"] >= len(self.t["reactions"]):
                self.err("reaction out of range")
            out.append({"damage": h["damage"], "requirements": r, "reactions": h["reactions"]})
            if not r or r[0][0] == REQ_END:
                return out
            i += 1
        self.err("hit_conditions unterminated")
        return out

    def props(self, i, table):
        if i is None:
            return []
        ps, out = self.t[table], []
        while i < len(ps):
            p = ps[i]
            q = {"id": p["id"], "params": p["params"], "requirements": self.reqs(p["requirements"])}
            if "frame" in p:
                q["frame"] = p["frame"]
            out.append(q)
            if p["id"] == (0 if table == "extra_move_props" else REQ_END):
                return out
            i += 1
        self.err(table + " unterminated")
        return out

    def move(self, mid, m, names):
        r = {"id": mid, "name": names.get(m["name"], ""), "name_hash": "0x%08X" % m["name"],
             "anim_name": names.get(m["anim"], ""), "anim_hash": "0x%08X" % m["anim"]}
        for k, v in m.items():
            if k not in ("name", "anim", "cancel", "hit_condition", "extra_props", "start_props", "end_props"):
                r[k] = v
        for k in ("vuln", "hitlevel", "move_key"):
            r[k] = "0x%08X" % m[k]
        r["cancels"] = self.cancels(m["cancel"])
        r["hit_conditions"] = self.hit_conditions(m["hit_condition"])
        r["extra_props"] = self.props(m["extra_props"], "extra_move_props")
        r["start_props"] = self.props(m["start_props"], "move_start_props")
        r["end_props"] = self.props(m["end_props"], "move_end_props")
        if m["voiceclip"] is not None and m["voiceclip"] >= len(self.t["voiceclips"]):
            self.err("voiceclip out of range")
        if m["transition"] < 0x8000 and m["transition"] >= len(self.t["moves"]):
            self.err("transition out of range")
        for c in r["cancels"]:
            mv = c["move"]
            if int(c["command"], 16) == 0x800D:        # group cancel: move = group list index
                if mv >= len(self.t["group_cancels"]):
                    self.err("group cancel out of range")
            elif int(c["command"], 16) == 0x8012:      # move field is not a move id here (values > move count)
                continue
            elif mv < 0x8000 and mv >= len(self.t["moves"]):
                self.err("cancel move out of range")
        return r


def check_live(moves, live):
    """Compare resolved moves with one live-dump moveset entry (retail_moveset_dump.py format)."""
    stats = {"moves": 0, "equal": 0}
    first = None
    for a, b in zip(moves, live["moves"]):
        stats["moves"] += 1
        mine = {"name_hash": a["name_hash"],
                "cancels": [{"command": c["command"], "requirements": c["requirements"], "move": c["move"],
                             "window": c["window"], "starting_frame": c["starting_frame"]}
                            for c in a["cancels"]],
                "hit_conditions": [{"damage": h["damage"], "requirements": h["requirements"]}
                                   for h in a["hit_conditions"]],
                "extra_props": [{"id": p["id"], "frame": p["frame"], "params": p["params"],
                                 "requirements": p["requirements"]} for p in a["extra_props"]],
                "start_props": [{"id": p["id"], "frame": None, "params": p["params"],
                                 "requirements": p["requirements"]} for p in a["start_props"]],
                "end_props": [{"id": p["id"], "frame": None, "params": p["params"],
                               "requirements": p["requirements"]} for p in a["end_props"]]}
        theirs = {k: b.get(k) for k in mine}
        theirs["cancels"] = [{k: v for k, v in c.items() if k != "option"} for c in theirs["cancels"]]
        for k in ("start_props", "end_props"):       # live dumper overran the 1100 terminator
            lst, cut = theirs[k], []
            for q in lst:
                cut.append(q)
                if q["id"] == REQ_END:
                    break
            theirs[k] = cut
        theirs["name_hash"] = theirs["name_hash"].upper().replace("0X", "0x")
        if mine == theirs:
            stats["equal"] += 1
        elif first is None:
            first = (a["id"], [k for k in mine if mine[k] != theirs[k]])
    stats["first_diff"] = first
    return stats


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inputs", nargs="+")
    ap.add_argument("--names", nargs="*", default=[])
    ap.add_argument("--out", required=True)
    ap.add_argument("--check-live")
    a = ap.parse_args()
    files = []
    for p in a.inputs:
        files += sorted(glob.glob(os.path.join(p, "*.motbin"))) if os.path.isdir(p) else [p]
    names = R.load_names(a.names)
    live = {}
    if a.check_live:
        for e in json.load(open(a.check_live)):
            live[len(e["moves"]), e["moves"][0]["name_hash"], e["moves"][-1]["name_hash"]] = e
    os.makedirs(a.out, exist_ok=True)
    for p in files:
        code = os.path.splitext(os.path.basename(p))[0]
        d = open(p, "rb").read()
        try:
            hdr, t = parse(d)
        except ValueError as e:
            print("%-5s skip: %s" % (code, e))
            continue
        res = Resolver(t)
        moves = [res.move(i, m, names) for i, m in enumerate(t["moves"])]
        tabs = {k: v for k, v in t.items() if k != "moves"}
        with open(os.path.join(a.out, code + ".json"), "w", encoding="utf-8") as fh:
            json.dump({"code": code, "header": hdr, "moves": moves, "tables": tabs}, fh, separators=(",", ":"))
        msg = "%-5s moves %5d cancels %6d reqs %6d props %6d list errors %s" % (
            code, len(moves), len(t["cancels"]), len(t["requirements"]), len(t["extra_move_props"]),
            res.errors or 0)
        key = (len(moves), moves[0]["name_hash"], moves[-1]["name_hash"])
        lk = [e for k, e in live.items() if k[0] == key[0] and k[1].upper() == key[1].upper().replace("0X", "0x").upper()
              and k[2].upper() == key[2].upper()]
        if lk:
            msg += " | live: %s" % check_live(moves, lk[0])
        print(msg)


if __name__ == "__main__":
    main()
