"""Inspect native T7 .mot asset metadata without modifying or redistributing assets.

Directory scans inspect .bin files and leave archives untouched.
Offsets in this profile are relative to the byte after the 0x2e8-byte header.
This is an asset-layout report, not validation of a running game's memory.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

ROOT = 0x2E8
TABLE = 0x150
LIMIT = 256 << 20
SCHEMA = (
    ("reactions", 112), ("requirements", 8), ("hit_conditions", 24), ("projectiles", 168),
    ("pushbacks", 16), ("pushback_extra", 2), ("cancels", 40), ("group_cancels", 40),
    ("cancel_extra", 4), ("extra_properties", 12), ("begin_properties", 16),
    ("end_properties", 16), ("moves", 176), ("voiceclips", 4), ("input_sequences", 16),
    ("inputs", 8), ("parry", 4), ("cameras", 12), ("throw_cameras", 16),
)

def inspect_bytes(data):
    if len(data) < ROOT or len(data) > LIMIT or data[4:7] != b"HID":
        raise ValueError("Not a supported HID T7 .mot asset header")
    size = len(data) - ROOT
    tables = []
    for index, (name, stride) in enumerate(SCHEMA):
        offset, count = struct.unpack_from("<QQ", data, TABLE + index * 16)
        if offset > size or count > (size - offset) // stride:
            raise ValueError(f"{name} table exceeds asset bounds")
        absolute = ROOT + offset
        tables.append(dict(name=name, data_offset=offset, file_offset=absolute, count=count,
                           stride=stride, extent=count * stride,
                           file_offset_mod_8=absolute % 8, file_offset_mod_16=absolute % 16))
    occupied = sorted((row for row in tables if row["count"]), key=lambda row: row["data_offset"])
    for previous, current in zip(occupied, occupied[1:]):
        previous["gap_after"] = current["data_offset"] - previous["data_offset"] - previous["extent"]
        if previous["gap_after"] < 0:
            raise ValueError("Overlapping native asset tables")
    motas = []
    for index in range(13):
        offset = struct.unpack_from("<Q", data, 0x280 + index * 8)[0]
        present = bool(offset and offset <= size - 20 and data[ROOT + offset:ROOT + offset + 4] == b"MOTA")
        motas.append(dict(index=index, data_offset=offset, has_mota_header=present))
    return dict(metadata_ok=True, profile="t7-hid-relative-asset", data_root=ROOT,
                bytes=len(data), sha256=hashlib.sha256(data).hexdigest(),
                tables=tables, physical_table_order=[row["name"] for row in occupied], motas=motas,
                runtime_base_address=None)

def inspect_file(path):
    with path.open("rb") as source:
        data = source.read(LIMIT + 1)
    report = inspect_bytes(data)
    report["file"] = str(path)
    return report

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    files = sorted(path for path in args.path.iterdir() if path.is_file() and path.suffix.lower() == ".bin") if args.path.is_dir() else [args.path]
    reports = []
    for path in files:
        try:
            reports.append(inspect_file(path))
        except (OSError, ValueError) as error:
            reports.append(dict(file=str(path), metadata_ok=False, error=str(error)))
    result = dict(files=len(reports), metadata_valid=sum(row["metadata_ok"] for row in reports),
                  reports=reports)
    encoded = json.dumps(result, indent=2)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded + "\n", encoding="utf-8")
    else:
        print(encoded)
    return 0 if result["metadata_valid"] == len(reports) else 1

if __name__ == "__main__":
    raise SystemExit(main())
