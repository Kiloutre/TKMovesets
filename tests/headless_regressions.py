"""Exercise the shipped executable through its headless command line."""
import json
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

APPLICATION = pathlib.Path(sys.argv.pop(1)).resolve()
FIXTURE_WRITER = pathlib.Path(sys.argv.pop(1)).resolve()

class HeadlessTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="tkmovesets-cli-")
        self.addCleanup(self.directory.cleanup)
        self.root = pathlib.Path(self.directory.name)
        self.source = self.root / "original.tkmvst"
        subprocess.run([str(FIXTURE_WRITER), "--write-fixture", str(self.source)], check=True, timeout=15)

    def run_app(self, *arguments, code=0):
        result = subprocess.run([str(APPLICATION), "--headless", *map(str, arguments)],
                                cwd=self.root, capture_output=True, text=True, encoding="utf-8", timeout=30)
        self.assertEqual(result.returncode, code, result.stdout + result.stderr)
        return json.loads(result.stdout)

    def test_native_validation_and_unicode_relative_path(self):
        name = self.root / "mouvements_日本語.tkmvst"
        name.write_bytes(self.source.read_bytes())
        report = self.run_app("validate", name.name)
        self.assertTrue(report[0]["ok"])
        self.assertEqual(report[0]["game_id"], 0)

    def test_invalid_inputs_and_exit_codes(self):
        self.run_app("unknown", code=2)
        self.run_app("convert", code=2)
        report = self.run_app("validate", "missing.tkmvst", code=1)
        self.assertFalse(report[0]["ok"])
        invalid = self.root / "short.tkmvst"
        invalid.write_bytes(b"TKM2")
        report = self.run_app("validate", self.source, invalid, code=1)
        self.assertTrue(report[0]["ok"])
        self.assertFalse(report[1]["ok"])

    def test_codecs_preserve_source_and_round_trip(self):
        original = self.source.read_bytes()
        for codec in ("none", "lz4", "lzma", "compact-zstd"):
            encoded = self.root / (codec + ".tkmvst")
            decoded = self.root / (codec + "-decoded.tkmvst")
            self.run_app("recompress", self.source, encoded, codec)
            self.run_app("validate", encoded)
            self.run_app("recompress", encoded, decoded, "none")
            self.assertEqual(decoded.read_bytes(), original)
            self.assertEqual(self.source.read_bytes(), original)

    def test_corrupt_block_offsets_are_rejected(self):
        damaged = bytearray(self.source.read_bytes())
        block_list = int.from_bytes(damaged[72:76], "little")
        damaged[block_list + 8:block_list + 16] = (2**64 - 1).to_bytes(8, "little")
        self.source.write_bytes(damaged)
        report = self.run_app("validate", self.source, code=1)
        self.assertFalse(report[0]["ok"])

    def test_semantic_failures_preserve_source_and_output(self):
        subprocess.run([str(FIXTURE_WRITER), "--write-semantic-fixture", str(self.source)], check=True, timeout=15)
        self.run_app("validate", self.source)
        damaged = bytearray(self.source.read_bytes())
        payload = int.from_bytes(damaged[80:84], "little")
        # T7 current_aliases follows the 40-byte info prefix and 56 original aliases.
        current_aliases = payload + 40 + 56 * 2
        damaged[current_aliases:current_aliases + 2] = (65535).to_bytes(2, "little")
        self.source.write_bytes(damaged)
        report = self.run_app("validate", self.source, code=1)
        self.assertIn("alias", report[0]["error"])
        output = self.root / "preserved.tkmvst"
        output.write_bytes(b"existing output")
        self.run_app("convert", self.source, output, code=1)
        self.run_app("recompress", self.source, output, "lz4", code=1)
        self.assertEqual(output.read_bytes(), b"existing output")
        self.assertEqual(self.source.read_bytes(), damaged)

    def test_animation_metadata_is_bounded(self):
        import struct
        animation = self.root / "animation.tkanimC8"
        payload = bytearray(36)
        struct.pack_into("<HHI", payload, 0, 0xc8, 1, 2)
        animation.write_bytes(payload)
        report = self.run_app("animation-info", animation)
        self.assertEqual(report[0]["frames"], 2)
        animation.write_bytes(payload[:-1])
        report = self.run_app("animation-info", animation, code=1)
        self.assertFalse(report[0]["ok"])

    def test_atomic_failure_preserves_existing_output(self):
        original = self.source.read_bytes()
        self.run_app("recompress", self.source, self.root, "lz4", code=1)
        self.assertEqual(self.source.read_bytes(), original)
        self.assertEqual(list(self.root.glob("tkm*.tmp")), [])

    def test_native_conversion_and_unsupported_format(self):
        output = self.root / "converted.tkmvst"
        self.run_app("convert", self.source, output, "lz4")
        self.run_app("validate", output)
        unsupported = bytearray(self.source.read_bytes())
        unsupported[64:66] = (65535).to_bytes(2, "little")
        self.source.write_bytes(unsupported)
        before = output.read_bytes()
        self.run_app("convert", self.source, output, code=1)
        self.assertEqual(output.read_bytes(), before)

    def test_storage_scan_handles_unknown_games_and_directories(self):
        (self.root / "backup.tkmvst").mkdir()
        (self.root / "partial.tkmvst").write_bytes(b"TKM2")
        unknown = bytearray(self.source.read_bytes())
        unknown[64:66] = (65535).to_bytes(2, "little")
        (self.root / "unknown.tkmvst").write_bytes(unknown)
        entries = self.run_app("list", self.root)
        entries = {pathlib.Path(row["file"]).name: row for row in entries}
        self.assertEqual(len(entries), 3)
        self.assertFalse(entries["partial.tkmvst"]["valid"])
        self.assertFalse(entries["unknown.tkmvst"]["editable"])
        self.assertTrue(entries["original.tkmvst"]["editable"])
        # A previously partial file becomes valid after its writer finishes.
        (self.root / "partial.tkmvst").write_bytes(self.source.read_bytes())
        entries = self.run_app("list", self.root)
        self.assertTrue(all(row["valid"] for row in entries))

    def test_retail_panm_export_import_round_trip(self):
        fixture = self.root / "panm"
        self.run_app("retail-panm-fixture", fixture)
        doc = fixture / "doc.tkmvst"
        source = doc.read_bytes()
        checks = self.run_app("check-panm-t8", fixture / "native.panm", fixture / "encoded.panm", fixture / "partial.panm", code=1)
        self.assertEqual([row["ok"] for row in checks], [True, True, False])
        # Export one move, then every distinct animation.
        self.assertEqual(self.run_app("export-panm-t8", doc, 0, self.root / "move0.panm")["instance"], 0x100)
        report = self.run_app("export-panms-t8", doc, self.root / "all")
        self.assertEqual(report["animations"], 2)
        manifest = json.loads((self.root / "all" / "animations.json").read_text(encoding="utf-8"))
        self.assertEqual((self.root / "all" / manifest[0]["file"]).read_bytes(), (self.root / "move0.panm").read_bytes())
        # Exported native PANM onto another move: reuses the binding.
        reused = self.run_app("import-panm-t8", doc, 1, self.root / "move0.panm", self.root / "reused.tkmvst")
        self.assertFalse(reused["custom"])
        # New PANM: stored as a custom asset under a new instance named after the file (no other move changes),
        # then exported back unchanged.
        custom = self.run_app("import-panm-t8", doc, 0, fixture / "native.panm", self.root / "custom.tkmvst", "-", "lz4")
        self.assertTrue(custom["custom"])
        self.assertFalse(custom["replaced"])
        self.assertGreaterEqual(custom["instance"], 0x10000)
        self.assertEqual(custom["animation_name"], "native")
        self.assertTrue(self.run_app("validate", self.root / "custom.tkmvst")[0]["ok"])
        self.run_app("export-panm-t8", self.root / "custom.tkmvst", 0, self.root / "back.panm")
        self.assertEqual((self.root / "back.panm").read_bytes(), (fixture / "native.panm").read_bytes())
        again = self.run_app("import-panm-t8", self.root / "custom.tkmvst", 1, self.root / "back.panm", self.root / "again.tkmvst")
        self.assertFalse(again["custom"])
        # Partial skeleton: rewritten over the document's native template instead of refused.
        rewritten = self.run_app("import-panm-t8", doc, 0, fixture / "partial.panm", self.root / "rewritten.tkmvst")
        self.assertTrue(rewritten["custom"] and rewritten["rewritten"])
        self.run_app("export-panm-t8", self.root / "rewritten.tkmvst", 0, self.root / "rewritten.panm")
        self.assertTrue(self.run_app("check-panm-t8", self.root / "rewritten.panm")[0]["ok"])
        # Rejections: garbage, bad move, output equal to input.
        (self.root / "garbage.panm").write_bytes(b"\0\0\0\0PANM" + bytes(64))
        self.assertFalse(self.run_app("import-panm-t8", doc, 0, self.root / "garbage.panm", self.root / "bad.tkmvst", code=1)["ok"])
        self.assertFalse(self.run_app("import-panm-t8", doc, 9, fixture / "native.panm", self.root / "bad.tkmvst", code=1)["ok"])
        self.assertFalse(self.run_app("import-panm-t8", doc, 0, fixture / "native.panm", doc, code=2)["ok"])
        self.assertFalse((self.root / "bad.tkmvst").exists())
        self.assertEqual(doc.read_bytes(), source)

    def test_outputs_naming_an_input_are_refused(self):
        fixture = self.root / "panm"
        self.run_app("retail-panm-fixture", fixture)
        doc, panm = fixture / "doc.tkmvst", fixture / "native.panm"
        source, animation, t7 = doc.read_bytes(), panm.read_bytes(), self.source.read_bytes()
        # Another case and a hard link name the same file: a lexical path comparison misses both.
        link = self.root / "link.tkmvst"
        os.link(doc, link)
        for output in (doc.with_name(doc.name.upper()), link):
            self.run_app("export-panm-t8", doc, 0, output, code=2)
            self.run_app("import-panm-t8", doc, 0, panm, output, code=2)
            self.run_app("set-profile", doc, output, "in_heat=1", code=2)
        upper_panm = panm.with_name(panm.name.upper())
        self.run_app("panm-reencode", panm, upper_panm, code=2)
        self.run_app("panm-to-t7", panm, upper_panm, "c8", code=2)
        self.run_app("t7-anim-to-panm", self.source, 0, doc, self.source.with_name(self.source.name.upper()), code=2)
        self.run_app("t7-anim-to-panm", self.source, 0, doc, doc.with_name(doc.name.upper()), code=2)
        self.assertEqual(doc.read_bytes(), source)
        self.assertEqual(panm.read_bytes(), animation)
        self.assertEqual(self.source.read_bytes(), t7)

    def test_unwritable_outputs_fail(self):
        fixture = self.root / "panm"
        self.run_app("retail-panm-fixture", fixture)
        panm = fixture / "native.panm"
        missing = self.root / "missing" / "output"
        self.assertTrue(self.run_app("panm-dump", panm, self.root / "dump.json")["ok"])
        self.assertFalse(self.run_app("panm-dump", panm, missing, code=1)["ok"])
        self.assertTrue(self.run_app("panm-skeleton-inc", panm, self.root / "skeleton.inc")["ok"])
        self.assertFalse(self.run_app("panm-skeleton-inc", panm, missing, code=1)["ok"])
        self.assertFalse(missing.exists())

if __name__ == "__main__":
    unittest.main()
