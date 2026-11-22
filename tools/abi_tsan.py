#!/usr/bin/env python3
"""Run the C ABI's verification set and the event stack under ThreadSanitizer, against a tsan test cache (stage C8, gap P2).

The C ABI promises release from any thread, deferred destruction on the UI
thread, begin_invoke from any thread and the conformance kit's thread
handover (gf_abi.h, "Threads"); ThreadSanitizer checks those promises on
Linux and macOS. The MinGW runtime on Windows has no ThreadSanitizer.

The instrumented GUI.Forms comes from gui_forms' tools/abi_tsan.py, which
builds a tree with -fsanitize=thread, packs it as the ``tsan`` kind of test
cache (gui_forms tools/test_cache.py) and runs this tool on the archive.
CI publishes no tsan cache, so --archive is required and is taken unpinned
(fetch_test_cache.py --unpinned). This tool configures
``.build/<platform>-tsan`` against the cache with its settings (the TSan
flags among them), builds only the programs the selected tests run, and
runs them with every report fatal:

    python3 tools/abi_tsan.py --archive A [--set all|c_abi|tsan] [--jobs N] [--tests PATTERN] [--skip-labels PATTERN]

The tests are those labelled ``c_abi`` (the C ABI, its conformance kit
including the sub-surface part; cmake/sections/c_abi_1.cmake) and ``tsan``
(the event stack, the dispatcher and timers, threading, and the sub-surface
and live-surface machinery; cmake/sections/main.cmake beside
gui_forms_dispatcher_tests). --set picks both (the default) or one label.
--tests narrows the run with a ctest -R pattern, and --skip-labels leaves
out tests whose labels match (ctest -LE). Programs that cannot host
ThreadSanitizer are left out: the C# test (the .NET host is not
instrumented, and TSan's runtime must be loaded at process start) and the
MSVC test (Windows only). Suppressions live in tools/sanitizer/tsan.supp,
which admits system-library frames only.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys

import ci_tests
import fetch_test_cache

ROOT: Path = Path(__file__).resolve().parents[1]
CACHE_KIND: str = 'tsan'
# The label sets --set chooses from.
LABEL_SETS: dict[str, tuple[str, ...]] = {
    'all': ('c_abi', 'tsan'),
    'c_abi': ('c_abi',),
    'tsan': ('tsan',),
}
# Tests of the labels that cannot run under ThreadSanitizer (see the docstring).
EXCLUDED: str = '^(gui_forms_abi_csharp|gui_forms_abi_msvc_consumer)$'
# Test properties whose needs a test's command does not name: such a test
# makes the whole tree build.
OPAQUE_PROPERTIES: frozenset[str] = frozenset({'DEPENDS', 'FIXTURES_REQUIRED', 'REQUIRED_FILES'})


def tsan_options(suppressions: Path) -> str:
    """TSAN_OPTIONS for the run: every report fatal, with the suppressions."""
    return ('halt_on_error=1:abort_on_error=0:print_summary=1:second_deadlock_stack=1:'
            'suppressions=' + str(suppressions))


def test_labels(test: dict) -> list[str]:
    """A ctest json-v1 test's labels."""
    labels: list[str] = []
    prop: dict
    for prop in test.get('properties', []):
        if prop.get('name') == 'LABELS':
            labels = list(prop.get('value', []))
    return labels


def select_tests(listing: dict, labels_chosen: tuple[str, ...]) -> list[dict]:
    """The listing's tests that carry one of labels_chosen, less EXCLUDED."""
    kept: list[dict] = []
    test: dict
    for test in listing.get('tests', []):
        labels: list[str] = test_labels(test)
        chosen: bool = any(label in labels for label in labels_chosen)
        if chosen and not re.search(EXCLUDED, test['name']):
            kept.append(test)
    return kept


def program_targets(tests: list[dict], build: Path) -> list[str] | None:
    """The build targets the tests run: the stem of each command's program
    when it lies in the build tree (a program merged into a runner keeps its
    own launcher target of that name, cmake/TestRunners.cmake). None when a
    test's needs cannot be read from its command (a program outside the
    tree, such as cmake or Python, or a property in OPAQUE_PROPERTIES): the
    caller then builds the whole tree."""
    targets: set[str] = set()
    root: Path = build.resolve()
    test: dict
    for test in tests:
        prop: dict
        for prop in test.get('properties', []):
            if prop.get('name') in OPAQUE_PROPERTIES:
                return None
        command: list[str] = test.get('command', [])
        if not command:
            return None
        program: Path = Path(command[0])
        if not program.is_absolute() or not program.resolve().is_relative_to(root):
            return None
        targets.add(program.stem)
    return sorted(targets)


def main(argv: list[str] | None = None) -> int:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--archive', type=Path, required=True,
                        help='the tsan test cache gui_forms tools/abi_tsan.py packed')
    parser.add_argument('--platform', default=fetch_test_cache.host_label())
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--set', choices=sorted(LABEL_SETS), default='all',
                        help='the labelled tests to run: c_abi, tsan, or both (all)')
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    parser.add_argument('--tests', default='', help='narrow the selected set with this ctest -R pattern')
    parser.add_argument('--skip-labels', default='', help='leave out tests with labels matching this ctest -LE pattern')
    arguments: argparse.Namespace = parser.parse_args(argv)
    if platform.system() not in ('Darwin', 'Linux'):
        print(f'abi_tsan: ThreadSanitizer runs on Linux and macOS only, not {platform.system()}', file=sys.stderr)
        return 2
    build: Path = (arguments.build_dir or ROOT / '.build' / (arguments.platform + '-' + CACHE_KIND)).resolve()
    try:
        cache: Path = fetch_test_cache.fetch(fetch_test_cache.Lock(fetch_test_cache.LOCK), arguments.platform,
                                             CACHE_KIND, (ROOT / '.cache' / 'test-cache').resolve(),
                                             arguments.archive, True)
    except (OSError, ValueError, KeyError) as error:
        print(f'abi_tsan: {error}', file=sys.stderr)
        return 1
    manifest: dict = json.loads((cache / 'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('kind') != CACHE_KIND:
        print(f'abi_tsan: {arguments.archive} is a {manifest.get("kind")} cache, not {CACHE_KIND}', file=sys.stderr)
        return 1
    configure: list[str] = ['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Ninja',
                            '-C', str(ci_tests.local_settings(cache, build)),
                            '-DGUI_FORMS_TEST_CACHE=' + cache.as_posix()]
    launcher: str | None = shutil.which('ccache')
    if launcher:
        configure.extend(f'-DCMAKE_{language}_COMPILER_LAUNCHER={launcher}' for language in ('C', 'CXX', 'OBJCXX'))
    labels_chosen: tuple[str, ...] = LABEL_SETS[arguments.set]
    try:
        ci_tests.run(configure)
        show: list[str] = ['ctest', '--test-dir', str(build), '--show-only=json-v1']
        if arguments.tests:
            show.extend(['-R', arguments.tests])
        if arguments.skip_labels:
            show.extend(['-LE', arguments.skip_labels])
        listing: dict = json.loads(subprocess.run(show, check=True, capture_output=True, text=True).stdout)
        kept: list[dict] = select_tests(listing, labels_chosen)
        if not kept:
            print('abi_tsan: no test selected', flush=True)
            return 0
        targets: list[str] | None = program_targets(kept, build)
        build_command: list[str] = ['cmake', '--build', str(build), '--parallel', str(arguments.jobs)]
        if targets is None:
            print(f'abi_tsan: {len(kept)} tests; building the whole tree', flush=True)
        else:
            print(f'abi_tsan: {len(kept)} tests; {len(targets)} programs', flush=True)
            build_command.extend(['--target', *targets])
        build_command.extend(['--', '-k', '0'])
        ci_tests.run(build_command)
        os.environ['TSAN_OPTIONS'] = tsan_options(ROOT / 'tools/sanitizer/tsan.supp')
        names: str = '|'.join(re.escape(test['name']) for test in kept)
        ci_tests.run(['ctest', '--test-dir', str(build), '--output-on-failure', '--timeout', '900',
                      '-R', '^(' + names + ')$'])
    except subprocess.CalledProcessError as error:
        return error.returncode or 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
