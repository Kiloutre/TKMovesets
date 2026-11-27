"""Retail Tekken 8 .motbin (from tkdata.bin, tkdata.py) -> move names and encrypted move fields.

Layout (retail build 26155785, evidence: docs/formats/motbin.md section "Retail on-disk"):
- Header 0x318; table pointers at 0x168 (reactions, count 0x178), then (ptr, count) pairs from
  0x180 every 0x10 in the CNT table order; pointers are relative to 0x318 (data base).
  Same layout as LukzB's in-memory `t8_offsetTable`.
- Move record 0x448. Six encrypted fields, each a 0x20-byte block of 8 u32 (= CNT plain field):
  0x00 name_hash (CNT 0x00), 0x20 anim_name_hash (0x04), 0x58 vuln (0x20), 0x78 hitlevel (0x24),
  0xD0 chara_id pair s16 -id | u16 id << 16 (0x60), 0xF0 move_key = Kamui(CODE + "_" + name) (0x64).
  Block word j = K[PERM[j]] ^ t (t = per-block noise) except one word, which holds
  value ^ K[PERM[j]]. K = the 16 tkdata footer XOR keys.
  (The in-memory (value, key) 8-byte pairs of t8layout.decrypt are made from these at load time.)

Usage:
    python motbin_retail.py DIR_OR_FILES... [--names name_keys.json ...] [--out DIR]
Writes OUT/<code>.csv (id, names, vuln, hitlevel, chara id, move_key + its check) and OUT/summary.csv.
"""
import argparse
import csv
import glob
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from tkdata import XOR_KEYS as K, getKamuiHashDigest  # noqa: E402

DATA_BASE = 0x318
MOVE_SIZE = 0x448
PERM = (4, 3, 2, 6, 15, 0, 1, 5)
TABLE_ORDER = ["reactions", "requirements", "hit_conditions", "projectiles", "pushbacks",
               "pushback_extradata", "cancels", "group_cancels", "cancel_extradata",
               "extra_move_props", "move_start_props", "move_end_props", "moves", "voiceclips",
               "input_sequences", "inputs", "move_key_list", "cameras", "throw_cameras", "dialogues"]
# name: 100 % vs the live dump; all six: vs the CNT plain fields of name-paired moves
# (35,576 moves, 15 characters: 100 / 98.2 / 99.4 / 99.7 / 100 / 100 %). docs/formats/tkdata.md section 3.
FIELDS = {"name": 0x00, "anim": 0x20, "vuln": 0x58, "hitlevel": 0x78, "chara": 0xD0, "move_key": 0xF0}


def decode_block(b, off):
    """-> (value, noise t) of an encrypted 0x20 block, or (None, None) if it doesn't fit the scheme."""
    w = struct.unpack_from("<8I", b, off)
    x = [w[j] ^ K[PERM[j]] for j in range(8)]
    for t in set(x):
        odd = [j for j in range(8) if x[j] != t]
        if len(odd) == 1:
            return x[odd[0]], t
        if not odd:          # value == noise: every word agrees
            return t, t
    return None, None


def tables(d):
    out = {"reactions": (struct.unpack_from("<Q", d, 0x168)[0], struct.unpack_from("<Q", d, 0x178)[0])}
    for i, n in enumerate(TABLE_ORDER[1:]):
        out[n] = struct.unpack_from("<QQ", d, 0x180 + 0x10 * i)
    return out


def moves(d):
    off, n = tables(d)["moves"]
    off += DATA_BASE
    res = []
    for i in range(n):
        r = off + i * MOVE_SIZE
        row = {"id": i}
        for f, fo in FIELDS.items():
            row[f], row[f + "_t"] = decode_block(d, r + fo)
        res.append(row)
    return res


PLACEHOLDER = re.compile(r"^nk[0-9A-F]{8}_*$")   # community dictionaries' "unknown name, length N"


def load_names(paths):
    """{hash: name} from JSON dictionaries, skipping placeholder entries (nk<hex>___)."""
    names = {}
    for p in paths:
        with open(p, encoding="utf-8") as fh:
            for k, v in json.load(fh).items():
                if not PLACEHOLDER.match(v):
                    names.setdefault(int(k), v)
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("inputs", nargs="+")
    ap.add_argument("--names", nargs="*", default=[])
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    files = []
    for p in a.inputs:
        files += sorted(glob.glob(os.path.join(p, "*.motbin"))) if os.path.isdir(p) else [p]
    names = load_names(a.names)
    os.makedirs(a.out, exist_ok=True)
    summ = []
    for p in files:
        code = os.path.splitext(os.path.basename(p))[0]
        d = open(p, "rb").read()
        if d[8:11] != b"TEK":
            print("skip", p)
            continue
        ms = moves(d)
        bad = sum(1 for m in ms if any(m[f] is None for f in FIELDS))
        hit = sum(1 for m in ms if m["name"] is not None and m["name"] in names)
        ahit = sum(1 for m in ms if m["anim"] is not None and m["anim"] in names)
        mk = mk_ok = 0
        hx = lambda v: "" if v is None else "0x%08X" % v
        with open(os.path.join(a.out, code + ".csv"), "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(["id", "name_hash", "name", "anim_hash", "anim_name", "vuln", "hitlevel",
                        "chara_id", "move_key", "move_key_ok"])
            for m in ms:
                nm = names.get(m["name"], "")
                ok = ""
                if nm and m["move_key"] is not None:
                    mk += 1
                    ok = int(getKamuiHashDigest(code.upper() + "_" + nm) == m["move_key"])
                    mk_ok += ok
                ch = m["chara"]
                w.writerow([m["id"], hx(m["name"]), nm, hx(m["anim"]), names.get(m["anim"], ""),
                            hx(m["vuln"]), hx(m["hitlevel"]), "" if ch is None else ch >> 16,
                            hx(m["move_key"]), ok])
        summ.append([code, len(ms), hit, ahit, bad, mk_ok, mk])
        print("%-5s moves %5d names %5d (%5.1f%%) anim %5d undecodable %d move_key ok %d/%d" % (
            code, len(ms), hit, 100.0 * hit / max(1, len(ms)), ahit, bad, mk_ok, mk))
    with open(os.path.join(a.out, "summary.csv"), "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["code", "moves", "names_resolved", "anim_names_resolved", "undecodable",
                    "move_key_ok", "move_key_checked"])
        w.writerows(summ)


if __name__ == "__main__":
    main()
