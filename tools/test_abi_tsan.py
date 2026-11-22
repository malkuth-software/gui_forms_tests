#!/usr/bin/env python3
"""Tests for abi_tsan.py's selection: the labelled tests, the exclusions,
and the programs to build (or the whole tree when a test's needs are opaque)."""
from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import abi_tsan  # noqa: E402


def entry(name: str, command: list[str], labels: list[str], **properties: object) -> dict:
    listed: list[dict] = [{'name': 'LABELS', 'value': labels}]
    key: str
    for key, value in properties.items():
        listed.append({'name': key, 'value': value})
    return {'name': name, 'command': command, 'properties': listed}


class SelectionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.directory = tempfile.TemporaryDirectory()
        self.build = Path(self.directory.name).resolve()
        program: str = str(self.build / 'gui_forms_abi_conformance')
        self.listing: dict = {'tests': [
            entry('gui_forms_abi_conformance', [program], ['c_abi', 'area_abi']),
            entry('gui_forms_timer_tests', [str(self.build / 'gui_forms_timer_tests')], ['tsan']),
            entry('gui_forms_abi_csharp', [str(self.build / 'gui_forms_abi_csharp')], ['c_abi']),
            entry('gui_forms_tooltip_tests', [str(self.build / 'gui_forms_tooltip_tests')], []),
        ]}

    def tearDown(self) -> None:
        self.directory.cleanup()

    def names(self, labels: tuple[str, ...]) -> list[str]:
        return [test['name'] for test in abi_tsan.select_tests(self.listing, labels)]

    def test_labels_choose_and_exclusions_leave_out(self) -> None:
        self.assertEqual(self.names(abi_tsan.LABEL_SETS['all']),
                         ['gui_forms_abi_conformance', 'gui_forms_timer_tests'])
        self.assertEqual(self.names(abi_tsan.LABEL_SETS['tsan']), ['gui_forms_timer_tests'])
        self.assertEqual(self.names(abi_tsan.LABEL_SETS['c_abi']), ['gui_forms_abi_conformance'])

    def test_programs_in_the_tree_are_targets(self) -> None:
        kept: list[dict] = abi_tsan.select_tests(self.listing, abi_tsan.LABEL_SETS['all'])
        self.assertEqual(abi_tsan.program_targets(kept, self.build),
                         ['gui_forms_abi_conformance', 'gui_forms_timer_tests'])

    def test_opaque_needs_build_the_whole_tree(self) -> None:
        script: dict = entry('gui_forms_abi_schema_catalog_current', ['/usr/bin/cmake', '-P', 'x.cmake'], ['c_abi'])
        self.assertIsNone(abi_tsan.program_targets([script], self.build))
        bare: dict = entry('gui_forms_abi_audit', ['python3', 'audit.py'], ['c_abi'])
        self.assertIsNone(abi_tsan.program_targets([bare], self.build))
        fixture: dict = entry('gui_forms_abi_exports', [str(self.build / 'gui_forms_abi_exports')], ['c_abi'],
                              FIXTURES_REQUIRED=['abi'])
        self.assertIsNone(abi_tsan.program_targets([fixture], self.build))

    def test_options_name_the_suppressions(self) -> None:
        options: str = abi_tsan.tsan_options(Path('/x/tsan.supp'))
        self.assertIn('halt_on_error=1', options)
        self.assertTrue(options.endswith('suppressions=/x/tsan.supp'))


if __name__ == '__main__':
    unittest.main()
