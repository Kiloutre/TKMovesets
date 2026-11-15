import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path

spec = importlib.util.spec_from_file_location("game_data_generator", Path(__file__).resolve().parents[1] / "tools/generate_game_static_data.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class GeneratorTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.fighters = self.root / "fighters.csv"
        self.moves = self.root / "moves.csv"
        self.characters = [dict(code="ant", chr_id="chr_0006", fighter="Jin Kazama", movelist_name_jp="仁",
                                playable="True", fighter_evidence="test", note="")]
        self.rows = [dict(code="ant", name_hash="0x00000001", name="move", anim_hash="0x00000002", anim_name="animation", chara_id="5")]

    def write(self, path, rows):
        with path.open("w", encoding="utf-8", newline="") as output:
            writer = csv.DictWriter(output, fieldnames=rows[0].keys())
            writer.writeheader()
            writer.writerows(rows)

    def build(self):
        self.write(self.fighters, self.characters)
        self.write(self.moves, self.rows)
        return generator.generate(self.fighters, self.moves)

    def test_identity_unicode_and_determinism(self):
        data = self.build()
        self.assertEqual(data, self.build())
        self.assertEqual(data["characters"][0]["id"], 6)
        self.assertEqual(data["characters"][0]["name_jp"], "仁")
        self.assertEqual(data["names"]["0x00000002"], "animation")
        self.assertEqual(data["statistics"]["move_rows"], 1)

    def test_unknown_names_are_not_invented(self):
        self.rows[0]["name"] = ""
        data = self.build()
        self.assertNotIn("0x00000001", data["names"])
        self.assertEqual(data["statistics"]["unnamed_move_rows"], 1)

    def test_observed_runtime_ids_preserve_asset_identity(self):
        cases = [("aml", 32, 27), ("jly", 29, 26), ("bbn", 36, 31),
                 ("crw", 28, 25), ("dog", 38, 33), ("cbr", 39, 34),
                 ("bee", 40, 35), ("okm", 41, 36), ("kgr", 42, 37), ("tgr", 43, 38),
                 ("knk", 44, 39), ("wkz", 45, 40), ("ker", 46, 41), ("usi", 47, 42), ("inu", 48, 43)]
        for code, asset_id, runtime_id in cases:
            with self.subTest(code=code):
                self.characters = [dict(self.characters[0], code=code, chr_id=f"chr_{asset_id:04d}")]
                self.rows[0]["code"] = code
                character = self.build()["characters"][0]
                self.assertEqual(character["id"], asset_id)
                self.assertEqual(character["chr_id"], f"chr_{asset_id:04d}")
                self.assertEqual(character["runtime_id"], runtime_id)

    def test_repeated_hash_same_name_is_deduplicated(self):
        self.rows.append(dict(self.rows[0]))
        self.assertEqual(len(self.build()["names"]), 2)

    def test_conflicting_hash_rejected(self):
        self.rows.append(dict(self.rows[0], name="conflicting"))
        with self.assertRaises(ValueError): self.build()

    def test_duplicate_character_rejected(self):
        self.characters.append(dict(self.characters[0], code="other"))
        with self.assertRaises(ValueError): self.build()

    def test_unknown_character_code_rejected(self):
        self.rows[0]["code"] = "missing"
        with self.assertRaises(ValueError): self.build()

    def test_unidentified_character_keeps_no_runtime_id(self):
        self.characters.append(dict(self.characters[0], code="test", chr_id="", fighter="", playable=""))
        data = self.build()
        self.assertIsNone(data["characters"][1]["id"])
        self.assertIsNone(data["characters"][1]["playable"])


if __name__ == "__main__":
    unittest.main()
