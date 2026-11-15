import importlib.util
from pathlib import Path
import struct
import unittest

module_path = Path(__file__).resolve().parents[1] / "tools" / "inspect_t7_assets.py"
spec = importlib.util.spec_from_file_location("t7_assets", module_path)
assets = importlib.util.module_from_spec(spec)
spec.loader.exec_module(assets)

def fixture():
    data = bytearray(assets.ROOT + 160)
    data[4:7] = b"HID"
    for index in range(19):
        struct.pack_into("<QQ", data, assets.TABLE + index * 16, 160, 0)
    struct.pack_into("<QQ", data, assets.TABLE, 32, 1)
    struct.pack_into("<QQ", data, assets.TABLE + 16, 144, 1)
    return data

class NativeAssetTests(unittest.TestCase):
    def test_relative_layout_is_reported_without_modification(self):
        data = fixture()
        original = bytes(data)
        report = assets.inspect_bytes(data)
        self.assertEqual(report["physical_table_order"], ["reactions", "requirements"])
        self.assertEqual(report["tables"][0]["file_offset"], assets.ROOT + 32)
        self.assertEqual(report["tables"][0]["gap_after"], 0)
        self.assertIsNone(report["runtime_base_address"])
        self.assertEqual(data, original)

    def test_all_truncated_prefixes_fail(self):
        data = fixture()
        for size in range(len(data)):
            with self.subTest(size=size), self.assertRaises(ValueError):
                assets.inspect_bytes(data[:size])

    def test_overflowed_counts_and_offsets_fail(self):
        for offset, count in ((32, 2**64 - 1), (2**64 - 1, 1)):
            data = fixture()
            struct.pack_into("<QQ", data, assets.TABLE, offset, count)
            with self.assertRaises(ValueError):
                assets.inspect_bytes(data)

    def test_overlapping_tables_fail(self):
        data = fixture()
        struct.pack_into("<QQ", data, assets.TABLE + 16, 80, 1)
        with self.assertRaisesRegex(ValueError, "Overlapping"):
            assets.inspect_bytes(data)

    def test_wrong_signature_fails(self):
        data = fixture()
        data[4:7] = b"BAD"
        with self.assertRaises(ValueError):
            assets.inspect_bytes(data)

if __name__ == "__main__":
    unittest.main()
