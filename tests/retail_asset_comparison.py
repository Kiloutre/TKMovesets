"""Synthetic checks for offline comparison: no game files or external parser required."""
import importlib.util
from pathlib import Path
import unittest
import struct
import tempfile

spec = importlib.util.spec_from_file_location('comparison', Path(__file__).resolve().parents[1] /
                                             'tools/compare_retail_t8_assets.py')
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)


def move(h, requirement, target=False):
    params = (7, 99, 0, 0) if target else (7,)
    end = (1100, (0, 0, 0, 0)) if target else (881, (0,))
    return dict(h=h, index=h, name=f'move{h}', anim=h + (100 if target else 0),
                cancels=[dict(key=(8, 16), index=4, reqs=[(requirement, params), end])],
                hits=[], props=[])


class ComparisonTests(unittest.TestCase):
    def test_preserves_all_parameters_and_deduplicates_shared_lists(self):
        evidence = {}
        c.compare([move(1, 3), move(2, 3)], [move(1, 4, True), move(2, 4, True)], 'ant', evidence)
        row = evidence['requirements', 3, 4]
        self.assertEqual(len(row['contexts']), 1)
        self.assertEqual(row['parameters'][((7,), (7, 99, 0, 0))], 2)
        self.assertFalse(row['witnesses'][0]['matching_animation_name'])

    def test_duplicate_names_or_cancel_keys_do_not_vote(self):
        for duplicate_names in (True, False):
            a, b = move(1, 3), move(1, 4, True)
            left = [a, a] if duplicate_names else [a]
            if not duplicate_names:
                a['cancels'] *= 2
            evidence = {}
            c.compare(left, [b], 'ant', evidence)
            self.assertEqual(evidence, {})

    def test_first_parameter_change_or_wrong_terminator_does_not_vote(self):
        for bad in ([(4, (8, 0, 0, 0)), (1100, (0, 0, 0, 0))],
                    [(4, (7, 0, 0, 0)), (999, (0, 0, 0, 0))]):
            a, b = move(1, 3), move(1, 4, True)
            b['cancels'][0]['reqs'] = bad
            evidence = {}
            c.compare([a], [b], 'ant', evidence)
            self.assertEqual(evidence, {})

    def test_walk_rejects_bad_indices_and_missing_terminators(self):
        for index in (0, 9):
            with self.assertRaises(ValueError):
                c.walk([dict(id=1)], index, lambda r: r['id'] == 0)
        self.assertEqual(c.walk([], None, lambda r: False), [])

    def test_variants_count_as_one_character_family(self):
        evidence = {}
        for code in ('swl', 'xxa', 'xxe'):
            c.compare([move(1, 3)], [move(1, 4, True)], code, evidence, family='t7_DEVIL_JIN')
        self.assertEqual(evidence['requirements', 3, 4]['characters'], {'t7_DEVIL_JIN'})

    def test_anmbin_zero_offsets_are_unavailable_and_present_blobs_require_magic(self):
        data = bytearray(0x180)
        struct.pack_into('<I', data, 4, 2)
        struct.pack_into('<I', data, 0x1c, 1)
        struct.pack_into('<Q', data, 0x38, 0x98)
        struct.pack_into('<Q', data, 0x68, 0x108)
        struct.pack_into('<QQ', data, 0x98, 7, 0)
        struct.pack_into('<QQ', data, 0xd0, 8, 0x110)
        struct.pack_into('<I', data, 0x108, 8)
        data[0x114:0x118] = b'PANM'
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'test.anmbin'
            path.write_bytes(data)
            row = c.animation_inventory(path, 1)['categories'][0]
            self.assertEqual(row['panm_identifiers'], 1)
            self.assertEqual(row['zero_data_offsets'], 1)
            data[0x114:0x118] = b'FAIL'
            path.write_bytes(data)
            with self.assertRaises(ValueError):
                c.animation_inventory(path, 1)


if __name__ == '__main__':
    unittest.main()
