"""Exercise the actual legacy Python importer in isolated, offline bytearrays.

No Addresses module or game process is loaded. Only loadMoveset is invoked.
Local character assets are read in place and are never added to the repository.
"""
import argparse
import importlib
import json
from pathlib import Path
import subprocess
import sys
import types


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("legacy_repository", type=Path)
    parser.add_argument("character_folder", type=Path)
    parser.add_argument("regression_executable", type=Path)
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--mota-mask", type=lambda value: int(value, 0), default=0xfff)
    args = parser.parse_args()
    repository = args.legacy_repository.resolve()
    character = args.character_folder.resolve()

    def forbidden(*args, **kwargs):
        raise RuntimeError("Game/process APIs are forbidden in this offline check")

    stub = types.ModuleType("Addresses")
    stub.game_addresses = {}
    for name in ("GameClass", "VirtualAllocEx", "VirtualFreeEx", "GetLastError"):
        setattr(stub, name, forbidden)
    for name in ("MEM_RESERVE", "MEM_COMMIT", "MEM_DECOMMIT", "MEM_RELEASE", "PAGE_EXECUTE_READWRITE"):
        setattr(stub, name, 0)
    sys.modules["Addresses"] = stub
    sys.path.insert(0, str(repository))
    legacy = importlib.import_module("motbinImport")

    class OfflineImporter(legacy.Importer):
        def __init__(self):
            self.regions = {}
            self.next_address = 0x100000000

        def allocateMem(self, size):
            address = self.next_address
            self.next_address += ((size + 4095) & ~4095) + 0x10000000
            self.regions[address] = bytearray(size)
            return address

        def find(self, address, size):
            for base, data in self.regions.items():
                if base <= address and address - base + size <= len(data):
                    return data, address - base
            raise RuntimeError(f"Legacy importer accessed unallocated memory: {address:#x}+{size}")

        def readBytes(self, address, size):
            data, offset = self.find(address, size)
            return bytes(data[offset:offset + size])

        def writeBytes(self, address, value):
            data, offset = self.find(address, len(value))
            data[offset:offset + len(value)] = value

        def readInt(self, address, bytes_length=4):
            return int.from_bytes(self.readBytes(address, bytes_length), "little")

        def writeInt(self, address, value, bytes_length=4):
            value &= (1 << (8 * bytes_length)) - 1
            self.writeBytes(address, value.to_bytes(bytes_length, "little"))

    importer = OfflineImporter()
    moveset = importer.loadMoveset(folderName=str(character), charactersPath=str(character.parent))
    destination = args.output_directory.resolve()
    destination.mkdir(parents=True, exist_ok=True)
    regions = []
    for index, (address, data) in enumerate(importer.regions.items()):
        filename = f"region-{index}.bin"
        (destination / filename).write_bytes(data)
        regions.append({"address": address, "file": filename})
    manifest = destination / "memory.json"
    manifest.write_text(json.dumps({"character": character.name, "moveset": moveset.motbin_ptr,
                                   "regions": regions, "mota_mask": args.mota_mask}, indent=2), encoding="utf-8")
    subprocess.run([str(args.regression_executable.resolve()), "--check-legacy-export", str(manifest)], check=True)


if __name__ == "__main__":
    main()
