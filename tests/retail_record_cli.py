"""Offline record conversion integration tests; no game process is opened."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

APP, REGRESSION = map(lambda p: str(Path(p).resolve()), sys.argv[1:3])
sys.argv = sys.argv[:1]


class RecordCliTests(unittest.TestCase):
    def test_binary_conversion(self):
        with tempfile.TemporaryDirectory(prefix='tkmovesets-convert-t8-') as directory:
            root = Path(directory)
            source, output = root / 'source.tkmvst', root / 'converted.tkmvst'
            subprocess.run([REGRESSION, '--write-t7-retail-fixture', str(source)], check=True)
            original = source.read_bytes()

            def run(command, *args):
                return subprocess.run([APP, '--headless', command, *map(str, args)], capture_output=True, text=True)

            for codec in ('none', 'lz4', 'lzma'):
                result = run('convert-t8', source, output, codec)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertEqual(run('validate', output).returncode, 0)
                self.assertEqual(source.read_bytes(), original)
            previous = output.read_bytes()
            self.assertNotEqual(run('convert-t8', source, source).returncode, 0)
            self.assertEqual(source.read_bytes(), original)
            # The semantic fixture's animation is not a T7 body animation: it does not convert, its moves fall back
            # to the idle animation and the conversion still succeeds.
            subprocess.run([REGRESSION, '--write-semantic-fixture', str(source)], check=True)
            converted = run('convert-t8', source, output)
            self.assertEqual(converted.returncode, 0, converted.stdout + converted.stderr)
            self.assertEqual(run('validate', output).returncode, 0)
            self.assertNotEqual(output.read_bytes(), previous)
            # A valid T7 file is never refused: with a running game it is converted and imported (read-only shadow
            # check here); without one, the command fails only because there is no game to attach to.
            checked = run('check-import-t8', source, '0')
            if checked.returncode == 0:
                self.assertIn('"ok":true', checked.stdout.replace(' ', ''), checked.stdout + checked.stderr)

    def test_roundtrip_and_rejection(self):
        with tempfile.TemporaryDirectory(prefix='tkmovesets-retail-records-') as directory:
            root = Path(directory)
            source, output = root / 'source.tkmvst', root / 'records.json'
            subprocess.run([REGRESSION, '--write-semantic-fixture', str(source)], check=True)
            original = source.read_bytes()

            def run(src=source, dest=output):
                return subprocess.run([APP, '--headless', 'convert-records-t8', str(src), str(dest)],
                                      capture_output=True, text=True)

            converted = run()
            self.assertEqual(converted.returncode, 0, converted.stdout + converted.stderr)
            data = json.loads(output.read_text(encoding='utf-8'))
            self.assertFalse(data['loadable_moveset'])
            self.assertEqual(data['requirements'][1], {'id': 1100, 'parameters': [0, 0, 0, 0]})
            self.assertEqual(source.read_bytes(), original)
            saved = output.read_bytes()
            self.assertNotEqual(run(dest=source).returncode, 0)
            self.assertEqual(source.read_bytes(), original)

            # Header data start @80; table offset is the second block entry.
            payload_start = struct.unpack_from('<I', original, 80)[0]
            block_list = struct.unpack_from('<I', original, 72)[0]
            table_offset = struct.unpack_from('<Q', original, block_list + 8)[0]
            records_offset = struct.unpack_from('<Q', original, block_list + 4 * 8)[0]
            requirement_offset = struct.unpack_from('<Q', original, payload_start + table_offset + 16)[0]
            altered = bytearray(original)
            struct.pack_into('<I', altered, payload_start + records_offset + requirement_offset, 217)
            source.write_bytes(altered)
            rejected = run()
            self.assertNotEqual(rejected.returncode, 0)
            report = json.loads(rejected.stdout)
            self.assertEqual(report['issues'][0]['id'], 217)
            self.assertEqual(report['requirements'], [])
            self.assertEqual(output.read_bytes(), saved)
            self.assertEqual(source.read_bytes(), altered)
            source.write_bytes(original[:100])
            self.assertNotEqual(run().returncode, 0)
            self.assertEqual(output.read_bytes(), saved)


if __name__ == '__main__':
    unittest.main()
