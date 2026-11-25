#!/usr/bin/env python3
"""Tests for ci_tests.py: the cache's settings remapped to this machine."""
from __future__ import annotations
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import ci_tests

RECORDED: str = 'D:/a/gui_forms/gui_forms/.toolchain/a6c66b10/msys64/clang64/lib/clang/22'


def settings_text(resource: str) -> str:
    """A Windows settings.cmake as GUI.Forms' CI records it with the own runtime."""
    return (f'set(CMAKE_CXX_FLAGS "-isystem \\"D:/a/gui_forms/gui_forms/.ci/own-runtime/libcxx/include/c++/v1\\" '
            f'-nostdinc -isystem \\"{resource}/include\\" -isystem \\"D:/a/gui_forms/gui_forms/.ci/own-runtime/'
            f'stxlib/src/hosted/windows/crt/include\\"" CACHE STRING "")\n'
            f'set(CMAKE_C_FLAGS "-nostdinc -isystem \\"{resource}/include\\"" CACHE STRING "")\n')


class ResourceDirectoryTests(unittest.TestCase):
    def test_a_recorded_directory_not_here_becomes_this_compilers(self) -> None:
        with tempfile.TemporaryDirectory() as scratch:
            root: Path = Path(scratch)
            (root / 'cache').mkdir()
            (root / 'cache' / 'settings.cmake').write_text(settings_text(RECORDED), encoding='utf-8')
            local: str = 'D:/a/gui_forms_tests/gui_forms_tests/.toolchain/a6c66b10/msys64/clang64/lib/clang/22'
            with mock.patch.object(ci_tests, 'local_resource_dir', return_value=local):
                text: str = ci_tests.local_settings(root / 'cache', root / 'build').read_text(encoding='utf-8')
            self.assertNotIn(RECORDED, text)
            self.assertEqual(text.count(f'-isystem \\"{local}/include\\"'), 2)
            # The own runtime's locations are not the resource directory's to change.
            self.assertIn('D:/a/gui_forms/gui_forms/.ci/own-runtime/libcxx', text)

    def test_a_recorded_directory_that_is_here_is_kept(self) -> None:
        with tempfile.TemporaryDirectory() as scratch:
            root: Path = Path(scratch)
            resource: Path = root / 'llvm' / 'lib' / 'clang' / '22'
            (resource / 'include').mkdir(parents=True)
            (resource / 'include' / 'stddef.h').write_text('', encoding='utf-8')
            (root / 'cache').mkdir()
            (root / 'cache' / 'settings.cmake').write_text(settings_text(resource.as_posix()), encoding='utf-8')
            with mock.patch.object(ci_tests, 'local_resource_dir', side_effect=AssertionError('not asked')):
                text: str = ci_tests.local_settings(root / 'cache', root / 'build').read_text(encoding='utf-8')
            self.assertIn(f'-isystem \\"{resource.as_posix()}/include\\"', text)

    def test_no_compiler_answer_leaves_the_flags(self) -> None:
        with tempfile.TemporaryDirectory() as scratch:
            root: Path = Path(scratch)
            (root / 'cache').mkdir()
            (root / 'cache' / 'settings.cmake').write_text(settings_text(RECORDED), encoding='utf-8')
            with mock.patch.object(ci_tests, 'local_resource_dir', return_value=None):
                text: str = ci_tests.local_settings(root / 'cache', root / 'build').read_text(encoding='utf-8')
            self.assertIn(RECORDED, text)


if __name__ == '__main__':
    unittest.main()
