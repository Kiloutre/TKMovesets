import importlib.util
import json
from pathlib import Path
import unittest
import copy

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('reference_data', ROOT / 'tools/generate_editor_reference_data.py')
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


def entry(game, category, identifier, label):
    return dict(game=game, category=category, id=identifier, label=label,
                parameter_or_value_range=0, description=None, notes={}, source={'row': identifier})


class ReferenceDataTests(unittest.TestCase):
    def test_generated_files_current(self):
        for name, content in generator.outputs().items():
            self.assertEqual((ROOT / name).read_text(encoding='utf-8'), content, name)

    def test_code_labels_fill_only(self):
        catalog = dict(target_game='t8-retail', target_build='26155785', mode='fill_missing', runtime_enabled=False,
                       labels=[dict(category='requirements', id=1, label='Code name'),
                               dict(category='requirements', id=2, label='New name'),
                               dict(category='properties', id=3, label='Other category')])
        self.assertEqual(generator.code_labels('1, Sheet label\n', catalog, 'requirements'),
                         '1, Sheet label\n2, New name\n')
        catalog['labels'].append(dict(category='requirements', id=2, label='Duplicate'))
        with self.assertRaises(ValueError):
            generator.code_labels('', catalog, 'requirements')

    def test_code_label_replacements_need_reviewed_text(self):
        catalog = dict(target_game='t8-retail', target_build='26155785', mode='fill_missing', runtime_enabled=False,
                       labels=[], replacements=[dict(category='requirements', id=1, label='Code', previous='Sheet')])
        self.assertEqual(generator.code_labels('1, Sheet\n', catalog, 'requirements'), '1, Code\n')
        with self.assertRaises(ValueError):
            generator.code_labels('1, Edited sheet\n', catalog, 'requirements')

    def test_ambiguous_targets_preserved(self):
        entries = [entry('t7', 'requirement', 12, 'Same'),
                   entry('t8-retail', 'requirement', 13, 'same'),
                   entry('t8-retail', 'requirement', 14, ' SAME ')]
        result = generator.mappings(entries)['entries'][0]
        self.assertEqual(result['status'], 'ambiguous')
        self.assertEqual([item['id'] for item in result['retail_candidates']], [13, 14])
        self.assertFalse(result['runtime_enabled'])
        self.assertIsNone(result['parameter_transform'])

    def test_same_id_is_not_evidence(self):
        entries = [entry('t7', 'requirement', 0, 'Always true'),
                   entry('t8-retail', 'requirement', 0, 'Different behavior'),
                   entry('t8-retail', 'extra_property', 1, 'Always true'),
                   entry('t8cnt', 'requirement', 2, 'Always true')]
        self.assertEqual(generator.mappings(entries)['entries'][0]['status'], 'unmapped')

    def test_missing_labels_do_not_match(self):
        entries = [entry('t7', 'extra_property', 1, None), entry('t8-retail', 'extra_property', 2, None)]
        self.assertEqual(generator.mappings(entries)['entries'][0]['retail_candidates'], [])

    def test_legacy_fallback_and_duplicates(self):
        entries = [entry('t7', 'extra_property', 1, 'Flag #2\nFirst'),
                   entry('t7', 'extra_property', 1, 'Second'), entry('t7', 'extra_property', 2, None)]
        text = generator.labels(entries, 't7', 'extra_property', {'1': 'Old', '2': 'Retained'})
        self.assertIn('1, Flag #2\\nFirst / Second\n', text)
        self.assertIn('2, Retained\n', text)
        self.assertNotIn('Old', text)

    def test_documented_resolutions(self):
        data = json.loads((ROOT / 'research/t7-to-retail-t8-mapping.json').read_text(encoding='utf-8'))
        resolved = [row for row in data['entries'] if row['status'] == 'documented_alignment']
        self.assertEqual(len(resolved), 17)
        for row in resolved:
            self.assertIn(row['selected_retail_id'], [target['id'] for target in row['retail_candidates']])
            self.assertIn('reason', row['resolution'])
            self.assertFalse(row['runtime_enabled'])
        rotation = next(row for row in resolved if row['category'] == 'extra_property' and row['t7_id'] == 0x8202)
        self.assertEqual(rotation['selected_retail_id'], 0x8272)
        # Identical descriptions for custom camera variants do not resolve the ambiguity.
        custom = next(row for row in data['entries'] if row['category'] == 'extra_property' and row['t7_id'] == 0x8005)
        self.assertEqual(custom['status'], 'ambiguous')

    def test_decision_validation(self):
        rows = [entry('t7', 'requirement', 1, 'a'), entry('t8-retail', 'requirement', 2, 'a')]
        with self.assertRaises(ValueError):
            generator.resolve(generator.mappings(rows), [dict(category='requirement', t7_id=1, retail_id=3)])

    def test_real_mapping_coverage_and_provenance(self):
        mapping = json.loads((ROOT / 'research/t7-to-retail-t8-mapping.json').read_text(encoding='utf-8'))
        self.assertEqual(mapping['target_game'], 't8-retail')
        self.assertEqual(len(mapping['entries']), 523)
        terminal = next(row for row in mapping['entries'] if row['category'] == 'requirement' and row['t7_id'] == 881)
        self.assertEqual([candidate['id'] for candidate in terminal['retail_candidates']], [1100])
        for row in mapping['entries']:
            self.assertIn('url', row['source'])
            self.assertFalse(row['runtime_enabled'])
            self.assertIsNone(row['parameter_transform'])
            for target in row['retail_candidates']:
                self.assertIn('url', target['source'])

    def test_build_pinned_alias_history_and_resolution(self):
        aliases = json.loads((ROOT / 'research/t7-to-retail-t8-aliases.json').read_text(encoding='utf-8'))
        self.assertEqual(aliases['target_build'], '26155785')
        self.assertEqual(len(aliases['entries']), 1239)  # 2026-11-14: handler-review overrides add ids
        row = next(r for r in aliases['entries'] if (r['category'], r['t7_id']) == ('extra_property', 0x835F))
        self.assertEqual(row['status'], 'conflict')
        self.assertEqual(row['effective_status'], 'handler_supported_id_alignment')
        self.assertEqual(row['review_retail_id'], 0x84A8)
        self.assertEqual(row['handler_resolution']['rejected_retail_id'], 0x83F5)
        self.assertEqual(sum(r['effective_status'] == 'conflict' for r in aliases['entries']), 1)  # the others resolved by handler review
        for r in aliases['entries']:
            self.assertFalse(r['runtime_enabled'])
            self.assertIsNone(r['parameter_transform'])

    def test_alias_enrichment_preserves_sheet_candidates_and_build(self):
        aliases = json.loads((ROOT / 'research/t7-to-retail-t8-aliases.json').read_text(encoding='utf-8'))
        mapping = generator.mappings([entry('t7', 'requirement', 0, 'Always true'),
                                      entry('t8-retail', 'requirement', 999, 'Always true')])
        result = generator.alias_evidence(mapping, aliases)
        self.assertEqual(result['target_build'], 'unspecified')
        self.assertEqual(result['entries'][0]['retail_candidates'][0]['id'], 999)
        self.assertEqual(result['entries'][0]['offline_alias_evidence']['review_retail_id'], 0)
        self.assertEqual(result['entries'][0]['offline_alias_evidence']['target_build'], '26155785')
        for change in ('cnt', 'runtime', 'parameter', 'duplicate'):
            bad = copy.deepcopy(aliases)
            if change == 'cnt': bad['target_game'] = 't8cnt'
            if change == 'runtime': bad['entries'][0]['runtime_enabled'] = True
            if change == 'parameter': bad['entries'][0]['parameter_transform'] = 'identity'
            if change == 'duplicate': bad['entries'].append(bad['entries'][0])
            with self.assertRaises(ValueError): generator.alias_evidence(copy.deepcopy(mapping), bad)

    def test_handler_labels_preserve_text_and_namespace(self):
        aliases = json.loads((ROOT / 'research/t7-to-retail-t8-aliases.json').read_text(encoding='utf-8'))
        original = '10,  Intentional leading space\n11, Existing label\\nSecond line\n'
        self.assertEqual(generator.handler_labels(original, aliases, 't7', 'requirement'), original)
        text = generator.handler_labels(original, aliases, 't8-retail', 'extra_property')
        self.assertIn('33960, Set secondary player flag bit 5 (purpose unknown)\n', text)
        self.assertIn('33781, Set conditional global flag (purpose unknown)\n', text)
        self.assertNotIn('33631,', text)
        self.assertNotIn('33754,', text)  # CNT 0x83DA must not receive the retail label.
        self.assertIn(original, text)
        with self.assertRaises(ValueError):
            generator.handler_labels('33960, Existing sheet label\n', aliases, 't8-retail', 'extra_property')
        bad = copy.deepcopy(aliases)
        bad['handler_resolution']['labels'][0]['game'] = 'cnt'
        with self.assertRaises(ValueError): generator.handler_labels(original, bad, 't7', 'extra_property')

    def test_asset_handler_reviews_cover_differences_and_new_candidates(self):
        review = json.loads((ROOT / 'research/retail-t8-handler-review.json').read_text())
        evidence = json.loads((ROOT / 'research/retail-t8-handler-evidence.json').read_text())
        generator.validate_handler_review(review, evidence)
        differences = [r for r in review['entries'] if r['original_comparison'] == 'disagrees']
        self.assertEqual(len(differences), 21)
        self.assertEqual(sum(r['status'] == 'difference_rejected' for r in differences), 20)
        self.assertFalse(any(r['status'].startswith('unresolved') for r in differences))
        camera = next(r for r in differences if r['status'] == 'difference_corrected')
        self.assertEqual((camera['source_id'], camera['selected_retail_id']), (0x83A2, 0x84EB))
        self.assertEqual(camera['previous_retail_id'], 0x821D)
        banks = evidence['stored_value_bank_controls']
        self.assertEqual([r['retail_offset'] for r in banks['slots']], [0x584, 0x588, 0x58C])
        supported = next(r for r in banks['asset_support'] if (r['source_id'], r['target_id']) == (0x160, 0x166))
        self.assertEqual(supported['independent_contexts'], 337)
        self.assertEqual(len(supported['characters']), 34)
        additions = [r for r in review['entries'] if r['original_comparison'] == 'previously_unselected']
        self.assertEqual(len(additions), 16)
        self.assertEqual(sum(r['status'] == 'handler_supported_alignment' for r in additions), 14)
        for row in review['entries']:
            self.assertFalse(row['runtime_enabled'])
            self.assertIsNone(row['parameter_transform'])
            if row['status'].startswith('unresolved'):
                self.assertIsNone(row['selected_retail_id'])

    def test_asset_handler_labels_reject_unsupported_evidence_and_overwrites(self):
        review = json.loads((ROOT / 'research/retail-t8-handler-review.json').read_text())
        evidence = json.loads((ROOT / 'research/retail-t8-handler-evidence.json').read_text())
        self.assertEqual(len(review['labels']), 18)
        for change in ('runtime', 'namespace', 'unsupported', 'coverage', 'transform', 'selection', 'wrong_handler'):
            bad = copy.deepcopy(review)
            if change == 'runtime': bad['runtime_enabled'] = True
            if change == 'namespace': bad['labels'][0]['game'] = 't8cnt'
            if change == 'unsupported': bad['entries'][bad['labels'][0]['review_index']]['status'] = 'unresolved_build_gap'
            if change == 'coverage': bad['entries'].pop()
            if change == 'transform': bad['entries'][0]['parameter_transform'] = 'identity'
            if change == 'selection': bad['entries'][0]['selected_retail_id'] = 99999
            if change == 'wrong_handler': bad['entries'][0]['evidence_handlers']['candidate'] = 'retail:req:0x0000'
            with self.assertRaises(ValueError): generator.validate_handler_review(bad, evidence)
        with self.assertRaises(ValueError):
            generator.reviewed_labels('357, Existing description\n', review, 'requirements')
        text = generator.reviewed_labels('1,  Keep leading space\n', review, 'requirements')
        self.assertIn('1,  Keep leading space\n', text)
        self.assertIn('357, Claudio Starburst\n', text)

    def test_handler_review_annotation_keeps_historical_sheet_candidates(self):
        review = json.loads((ROOT / 'research/retail-t8-handler-review.json').read_text())
        mapping = generator.mappings([entry('t7', 'extra_property', 0x83A2, 'Camera'),
                                      entry('t8-retail', 'extra_property', 0x821D, 'Camera')])
        result = generator.annotate_handler_reviews(mapping, review)['entries'][0]
        self.assertEqual(result['retail_candidates'][0]['id'], 0x821D)
        self.assertEqual(result['offline_handler_reviews'][0]['selected_retail_id'], 0x84EB)
        self.assertFalse(result['runtime_enabled'])


if __name__ == '__main__':
    unittest.main()
