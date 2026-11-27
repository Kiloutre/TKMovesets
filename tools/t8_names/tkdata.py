"""Tekken 8 tkdata.bin / dlc_data_*.bin (`__TEKKEN8FILES__` VFS archive) reader, static.

Format (retail build 26155785; method from umin135/Polaris_TKDataModEditor
src/extract/TkdataExtractor.cpp, reimplemented here; see docs/formats/tkdata.md):
- 0x00: magic "__TEKKEN8FILES__". File data starts at 0x10 (entry offsets are relative to it).
- Last 128 bytes = footer, XOR'd with 16 u32 keys. Footer +0x11 u8 aes seed, +0x18 i32 toc offset,
  +0x20 i32 toc size, +0x28 i32 toc uncompressed size.
- TOC = AES-256-ECB(key(seed)) + PKCS7, then zstd. TOC +0x08 i32 count, +0x10 i32 first entry,
  entries 64 bytes: +8 u8 flag, +9 u8 key seed, +16 u64 name hash (Kamui hash of the VFS path,
  e.g. "mothead/bin/grl.motbin"), +24 u64 offset, +32 u64 stored size, +40 u64 size2.
- File = AES-256-ECB(key(entry seed)) + PKCS7; zstd if it starts with 28 B5 2F FD
  (flag 0: only the first size2 bytes are the zstd frame).
- key(seed) = KEY_POOL[(44983 * seed) % (len(KEY_POOL) - 32)][:32].

Usage:
    python tkdata.py ARCHIVE --list OUT.csv [--names DICT.json ...]
    python tkdata.py ARCHIVE --extract OUTDIR [--names DICT.json ...] [--match REGEX] [--all]
Names: JSON files {hash(decimal): name}; also generated for mothead/<sub>/<code>.<ext> from --codes.
"""
import argparse
import csv
import json
import os
import re
import struct
import sys

import zstandard
from Crypto.Cipher import AES

from kamuiHash import getKamuiHashDigest  # noqa: E402

MAGIC = b"__TEKKEN8FILES__"
XOR_KEYS = [
    0xADFB76C8, 0x7DEF1F1C, 0xA84B71E0, 0xD88448A2, 0x964F5B9E, 0x7EE2BC2C, 0xA27D5221, 0xBB9FE67D,
    0xAD15269F, 0xEC1A9785, 0x9BAE2F45, 0xA4296896, 0x275AA004, 0x37E22F31, 0x3803D4A7, 0x9B81329F,
]
KEY_POOL = (
    "U%ZPP0S2y4WoIofdsjM@2a%LJr@#*Ir!u8?FTSD2T%YcYsYl3I0W8=tSiI9.EOTQJ4?"
    "09hmYC3L5FL0uzRHN2?JF9IX9WI7t?UAQ4FujsC3XH+RkqIGJ%k19$OJS&.AKh$@LFT1"
    "C83nXrjJ0MMcZVWF0Ln!SfhwzSKoWDjQj7c!PXVEYOKD@+BUZGhY5@#RgW+Gw0sKw63Q"
    "BjAp$$QOEFB%$V5?jcS@Q+hfg!=DrtmmALG0G@UXXEaVr17bnLrDnovHG.%PfIyB!8W9n"
    "&#xAywcKo6te3DAYi$KSCCz8bXQiN331x77uw5IJVq.BUfckF4sEBp%XV+e6rFg*V.ZS40"
    "#supW8T3ZB4e892MpxUMPMxr8d4LCltyVlJHBWKjK2W4SlynsF1CK5CxAkwLd6?ta.W6c"
    "W4IrQ1f@x5tUvpfPxOOyao6kEjfaPKGsBYB=Y6qQwQW3l3oGtV5zEH=&EgBL8h7Zz5sm3"
    "L&HsrFLS8CKoh3lR.XBAWXnB%S*O5&q?H9zjOqqr#JqwX3TU%2hYHTJ&2dY*fWIrPdc!P"
    "$hPlzFZUQPkgy6+Pg5C8f#bYYXua9gDn484!XO=5BuHI2FX$RR8%&rX%c@6u8EFBYXLhi"
    "0BNW!w7d+Z=iOabHqZZXSQ6Db*sncGALQFLgm%lvHfKhMP5.78Gm0Dd?nFE9zsTBIF=f6"
    "VBtD=*1PWxmnthUSjGQ&tf@9l$CAJR87UM4I$2oa@8ncr48dDMyMuM7KZX4@B.Ny6dObQe"
    "SdJ2=8jkV61GvIBOHAbbUiWfDCu52&MRs=@Bz#HHUMoQ9WFm5TxKWdoHlDEOW$!4$z==V"
    "G9PkfxPIT4.y*737zPZ9V4?JG.rl7kHK$k44$hdf.L*QX.=wp&JbpULUm#GI2vJv*lPJ$"
    "2B7M9Gg.+.O6tgUZvU4HMV2&7gvcBHDA.FG9jzC@s5*6XW8mUHB6j$*tz$OVPYDjW@07a"
    "pt&poihk.I69&*d%y=m%@XP1ERk+bTP0v5iHd?xQ9C%1O2t=Tj7.xpgABlCyee1i1hPVz"
    "egGXSQD1Fp1lQd*IxwpA%HtBmuG6c$#43S?HK*Q*!Tj0kAU&RTbDoRfW=!MwGiHM2KMMN"
    "NBK=31f#83gt%NXAzix3D!6EQ5O3NSZKvQYvSKuvUSu1gGAATF1MqWG7USVjG9NAjz1&?K"
    "W0dTMaCJfD!Luym?WWLN3nf9#UcDUvL02m7L2$zJA=dbNQ2N5kVybBg7JE3ByS3AaZO=u"
    "@ty2g*dPrYW7UN&pqA1g*rQ+7%PM3XTsLcH=B+BFRIDPTsLj!+=68T+%vIMaRzFoiC4rJ"
    "A#uevTsalLrpO.WfY2SG%%qbKihqRXg2=IJwXKZ+xoYUydgLLh9eRt+Df0&5&C3T7i9sj"
    ".Z2TEUCxaNl2T8d@=Muw$S955+iHs=KCt?LZNFy*#%UU8gOHl*I9lQUXH4#cVYy9JY@gi"
    "lIQ?v.TdkUBN9gHob*OVicLITzNqQpGL6wvupDkVqTlAORJTgL5RMKqQYpSD8LiYUp#aW"
    "KgFCBs1hCYTzFwa$*cllNQW.M2GSSI+x#qa#JcK7Cwh.v78hI1&T3c#DR&VDazL.XL2KG"
    "USSBKmcc31R%JCa9736gp=QV5#I7%BGEdJkOs7QhKxt0n3sS4#sAl1BGNNXPx&9PoPx8.G"
    "dv=eDotAfJmKuSUAirWB0XUe4FG9PYTgR??$t2IKh4+LjNCZ8TkXGqh=7rN4B86TZ3gjJ"
    "6QY#sT?FIc?Z*IstFG43YZqw%BwC&Ml2p7LB@JH!YW!zvNJYKjgaNm1!%sf$ZQbms4Akc"
    "Ojs2Bh"
).encode("ascii")
ZSTD_MAGIC = b"\x28\xB5\x2F\xFD"


def aes_key(seed):
    off = (44983 * seed) % (len(KEY_POOL) - 32)
    return KEY_POOL[off:off + 32]


def unpad(b):
    p = b[-1] if b else 0
    if 1 <= p <= 16 and b[-p:] == bytes([p]) * p:
        return b[:-p]
    return b


def aes_dec(seed, data):
    if len(data) % 16:
        data += b"\0" * (16 - len(data) % 16)
    return unpad(AES.new(aes_key(seed), AES.MODE_ECB).decrypt(data))


def unzstd(data, cap=None):
    d = zstandard.ZstdDecompressor()
    try:
        return d.decompress(data)
    except zstandard.ZstdError:
        return d.decompress(data, max_output_size=cap or len(data) * 16)


class Archive:
    def __init__(self, path):
        self.path = path
        self.f = open(path, "rb")
        if self.f.read(16) != MAGIC:
            raise ValueError("bad magic")
        self.f.seek(0, 2)
        self.size = self.f.tell()
        self.f.seek(self.size - 128)
        foot = bytearray(self.f.read(128))
        for i, k in enumerate(XOR_KEYS):
            for j in range(4):
                foot[i * 4 + j] ^= (k >> (8 * j)) & 0xFF
        self.footer = bytes(foot)
        seed = foot[0x11]
        toc_off, toc_sz, toc_us = (struct.unpack_from("<i", foot, o)[0] for o in (0x18, 0x20, 0x28))
        self.f.seek(toc_off)
        toc = self.f.read(toc_sz)
        if seed:
            toc = aes_dec(seed, toc)
        if not toc.startswith(ZSTD_MAGIC):
            raise ValueError("TOC is not zstd")
        toc = unzstd(toc, toc_us)
        self.toc_sig = toc[:8]
        count, start = struct.unpack_from("<i", toc, 8)[0], struct.unpack_from("<i", toc, 0x10)[0]
        self.entries = []
        for i in range(count):
            e = toc[start + i * 64:start + i * 64 + 64]
            self.entries.append(dict(
                index=i, flag=e[8], seed=e[9], hash=struct.unpack_from("<Q", e, 16)[0],
                offset=struct.unpack_from("<Q", e, 24)[0], size=struct.unpack_from("<Q", e, 32)[0],
                size2=struct.unpack_from("<Q", e, 40)[0], raw=e.hex()))

    def read(self, e):
        if not e["size"]:
            return b""
        self.f.seek(0x10 + e["offset"])
        data = aes_dec(e["seed"], self.f.read(e["size"]))
        if data.startswith(ZSTD_MAGIC):
            comp = data[:e["size2"]] if e["flag"] == 0 and 0 < e["size2"] < len(data) else data
            return unzstd(comp)
        if 0 < e["size2"] < len(data):
            data = data[:e["size2"]]
        return data


def load_names(paths, codes):
    names = {}
    for p in paths:
        with open(p, encoding="utf-8") as fh:
            for k, v in json.load(fh).items():
                names[int(k)] = v
    for c in codes:
        for n in ("mothead/bin/%s.motbin", "mothead/bin/%s.anmbin", "mothead/bin/%s.stllstb",
                  "mothead/movelist/%s.mvl", "mothead/bin/%s.mvl"):
            n = n % c
            names.setdefault(getKamuiHashDigest(n), n)
    return names


def guess_ext(b):
    if b[:4] == b"\x00\x00\x00\x00" and len(b) > 0x10 and b[8:11] == b"TEK":
        return ".motbin"
    if b[8:11] == b"TEK":
        return ".motbin"
    return ".bin"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("archive")
    ap.add_argument("--list")
    ap.add_argument("--extract")
    ap.add_argument("--names", nargs="*", default=[])
    ap.add_argument("--codes", default="", help="comma-separated character codes to generate mothead names")
    ap.add_argument("--match", help="regex on the resolved name")
    ap.add_argument("--all", action="store_true", help="also extract unnamed entries (as hash.bin)")
    a = ap.parse_args()
    arc = Archive(a.archive)
    names = load_names(a.names, [c for c in a.codes.split(",") if c])
    named = sum(1 for e in arc.entries if e["hash"] in names)
    print("%s: %d entries, %d named, toc sig %r" % (a.archive, len(arc.entries), named, arc.toc_sig))
    if a.list:
        with open(a.list, "w", newline="", encoding="utf-8") as fh:
            w = csv.writer(fh)
            w.writerow(["index", "hash", "name", "flag", "seed", "offset", "size", "size2"])
            for e in arc.entries:
                w.writerow([e["index"], e["hash"], names.get(e["hash"], ""), e["flag"], e["seed"],
                            e["offset"], e["size"], e["size2"]])
    if a.extract:
        rx = re.compile(a.match) if a.match else None
        n = 0
        for e in arc.entries:
            name = names.get(e["hash"])
            if name is None and not a.all:
                continue
            if rx and not rx.search(name or ""):
                continue
            data = arc.read(e)
            rel = name or ("_unnamed/%08X%s" % (e["hash"], guess_ext(data)))
            out = os.path.join(a.extract, rel)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            with open(out, "wb") as fh:
                fh.write(data)
            n += 1
        print("extracted %d files to %s" % (n, a.extract))


if __name__ == "__main__":
    main()
