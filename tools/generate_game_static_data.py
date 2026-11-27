"""Generate optional retail name data from the local research CSVs (no game access)."""
import argparse
import csv
import hashlib
import json
from pathlib import Path

# User-observed player IDs. Asset IDs remain unchanged for animation lookup.
RUNTIME_CHARACTER_IDS = {"aml": 27, "jly": 26, "bbn": 31, "crw": 25,
                         "dog": 33, "cbr": 34, "bee": 35, "okm": 36,
                         "kgr": 37, "tgr": 38, "knk": 39, "wkz": 40, "ker": 41, "usi": 42, "inu": 43,
                         # Reina: LukzB's additionalReqDetails.py; the zbr moveset tests fighter id 28 with
                         # requirements 220/221/224/225 23 times (RE project findings log 2026-11-14).
                         "zbr": 28}


def read_csv(path):
    with path.open(encoding="utf-8-sig", newline="") as source:
        return list(csv.DictReader(source))


def generate(fighters, moves, extra_names=None):
    characters, ids, codes = [], set(), set()
    for row in read_csv(fighters):
        code, chr_id = row["code"], row["chr_id"]
        if not code or code in codes:
            raise ValueError(f"Duplicate or empty fighter code: {code}")
        codes.add(code)
        runtime_id = int(chr_id[4:]) if chr_id.startswith("chr_") else None
        if runtime_id is not None and not 0 <= runtime_id <= 0xffffffff:
            raise ValueError(f"Fighter id outside range: {runtime_id}")
        if chr_id and (runtime_id is None or chr_id != f"chr_{runtime_id:04d}"):
            raise ValueError(f"Invalid chr_ identifier: {chr_id}")
        if runtime_id is not None:
            if runtime_id in ids:
                raise ValueError(f"Conflicting fighter id: {runtime_id}")
            ids.add(runtime_id)
        playable = row["playable"].lower()
        if playable not in ("true", "false", ""):
            raise ValueError("Invalid playable flag")
        characters.append(dict(code=code, id=runtime_id, chr_id=chr_id,
                               name=row["fighter"], name_jp=row["movelist_name_jp"],
                               playable=None if not playable else playable == "true",
                               evidence=row["fighter_evidence"], note=row["note"],
                               move_count=int(row["moves"]) if row.get("moves") else None,
                               named_move_count=int(row["names_resolved"]) if row.get("names_resolved") else None))
        if code in RUNTIME_CHARACTER_IDS:
            characters[-1]["runtime_id"] = RUNTIME_CHARACTER_IDS[code]
    names, unresolved, count = {}, 0, 0
    for row in read_csv(moves):
        if row["code"] not in codes:
            raise ValueError(f"Unknown fighter code: {row['code']}")
        count += 1
        unresolved += not bool(row["name"])
        for hash_column, name_column in (("name_hash", "name"), ("anim_hash", "anim_name")):
            value = int(row[hash_column], 16)
            if not 0 <= value <= 0xffffffff:
                raise ValueError("Hash exceeds 32 bits")
            name = row[name_column]
            if not name:
                continue
            if value in names and names[value] != name:
                raise ValueError(f"Ambiguous name hash 0x{value:08X}")
            names[value] = name
    # Optional names of hashes that no move row uses ({"0x%08X": name}), e.g. the rest of a public dictionary.
    # A row name always wins; a different name for a row hash is an error, so conflicts are settled before this step.
    if extra_names:
        for key, name in json.loads(extra_names.read_text(encoding="utf-8")).items():
            value = int(key, 16)
            if not 0 <= value <= 0xffffffff or not name:
                raise ValueError(f"Invalid extra name entry: {key}")
            if names.setdefault(value, name) != name:
                raise ValueError(f"Extra name conflicts with a row name: 0x{value:08X}")
    return dict(schema_version=1, profile="t8-retail",
                sources=[dict(file=p.name, sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                         for p in (fighters, moves, extra_names) if p],
                statistics=dict(fighters=len(characters), move_rows=count,
                                unnamed_move_rows=unresolved, name_hashes=len(names)),
                characters=sorted(characters, key=lambda c: c["code"]),
                names={f"0x{key:08X}": names[key] for key in sorted(names)})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fighters", type=Path, required=True)
    parser.add_argument("--moves", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path(__file__).resolve().parents[1] /
                        "TKMovesets2/Resources/TKMInterface/t8-retail_names.json")
    parser.add_argument("--extra-names", type=Path, help="JSON of names for hashes that no move row uses")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    data = (json.dumps(generate(args.fighters, args.moves, args.extra_names), ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    if args.check:
        if not args.output.exists() or args.output.read_bytes() != data:
            raise SystemExit("Static name data is out of date")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        temporary = args.output.with_suffix(args.output.suffix + ".tmp")
        temporary.write_bytes(data)
        temporary.replace(args.output)
    print(f"{args.output}: {len(data):,} bytes")


if __name__ == "__main__":
    main()
