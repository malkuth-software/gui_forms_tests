#!/usr/bin/env python3
"""Build and run GUI.Forms' tests against the pinned test cache: CI's entry
point, and the one to run locally (same steps, same inputs).

Steps:
  1. fetch the test cache gui_forms.lock names (tools/fetch_test_cache.py),
     or take --archive;
  2. configure the tests with the cache's settings (cmake -C
     <cache>/settings.cmake: its toolchain file, build type and flags) and
     GUI_FORMS_TEST_CACHE, with ccache as the compiler launcher when present;
  3. build every test program, or only those --changed selects;
  4. run ctest over the same selection.

Selection (--changed BASE): the files that differ from BASE (git diff
BASE...HEAD, plus the working tree). When every one is a source of a test
program (CMake's file API names the targets that compile it), only those
programs are built and only the tests that run them are run. Anything else
changed (a header, a fixture, the CMake, gui_forms.lock, a tool) selects the
whole suite, as does --all (checkpoints) or no --changed.

The build tree is .build/<platform>[-sanitize] (--build-dir to change it).
A failed step stops the run with that step's status (a failed build after
compiling and linking everything it could, ninja -k 0); the build and test
logs stay in the build tree.
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

import fetch_test_cache

ROOT: Path = Path(__file__).resolve().parents[1]
SOURCE_SUFFIXES: frozenset[str] = frozenset({'.c', '.cc', '.cpp', '.cxx', '.m', '.mm'})


def run(command: list[str], **options: object) -> None:
    """Runs a command, echoing it; raises on failure."""
    print('+ ' + ' '.join(command), flush=True)
    subprocess.run(command, check=True, **options)  # type: ignore[arg-type]


def changed_files(base: str) -> list[str] | None:
    """The repository-relative paths that differ from base (committed or not),
    or None when git cannot say (the caller then runs the whole suite).

    A failing git command is reported with its own message: it used to end the
    run with git's exit status (128) and nothing printed."""
    listing: set[str] = set()
    command: list[str]
    for command in (['git', 'diff', '--name-only', base + '...HEAD'], ['git', 'diff', '--name-only', 'HEAD'],
                    ['git', 'ls-files', '--others', '--exclude-standard']):
        completed: subprocess.CompletedProcess[str] = subprocess.run(command, cwd=ROOT, check=False,
                                                                     capture_output=True, text=True)
        if completed.returncode != 0:
            print(f'selection: {" ".join(command)} failed ({completed.returncode}) in {ROOT}: '
                  f'{completed.stderr.strip()}', flush=True)
            return None
        listing.update(line for line in completed.stdout.splitlines() if line)
    return sorted(listing)


def write_file_api_query(build: Path) -> None:
    """Asks CMake's file API for the code model at the next configure."""
    query: Path = build / '.cmake' / 'api' / 'v1' / 'query' / 'codemodel-v2'
    query.parent.mkdir(parents=True, exist_ok=True)
    query.touch()


def source_targets(build: Path) -> dict[str, set[str]]:
    """Each source file (repository-relative) and the targets compiling it."""
    reply: Path = build / '.cmake' / 'api' / 'v1' / 'reply'
    indexes: list[Path] = sorted(reply.glob('index-*.json'))
    if not indexes:
        raise RuntimeError('the configure wrote no file API reply')
    index: dict = json.loads(indexes[-1].read_text(encoding='utf-8'))
    codemodel_file: str = index['reply']['codemodel-v2']['jsonFile']
    codemodel: dict = json.loads((reply / codemodel_file).read_text(encoding='utf-8'))
    mapping: dict[str, set[str]] = {}
    configuration: dict = codemodel['configurations'][0]
    entry: dict
    for entry in configuration['targets']:
        target: dict = json.loads((reply / entry['jsonFile']).read_text(encoding='utf-8'))
        source: dict
        for source in target.get('sources', []):
            path: Path = Path(source['path'])
            if path.is_absolute():
                try:
                    path = path.relative_to(ROOT)
                except ValueError:
                    continue
            mapping.setdefault(path.as_posix(), set()).add(target['name'])
    return mapping


def select(build: Path, base: str) -> tuple[list[str], list[str]] | None:
    """The programs to build and the test names to run for the changes since
    base, or None for the whole suite."""
    changes: list[str] | None = changed_files(base)
    if changes is None:
        print('selection: the changes are unknown; the whole suite', flush=True)
        return None
    if not changes:
        print('selection: nothing changed; the whole suite', flush=True)
        return None
    mapping: dict[str, set[str]] = source_targets(build)
    programs: set[str] = set()
    path: str
    for path in changes:
        if Path(path).suffix not in SOURCE_SUFFIXES or path not in mapping:
            print(f'selection: {path} is not a test program\'s source; the whole suite', flush=True)
            return None
        target: str
        for target in mapping[path]:
            # A program merged into a runner compiles as <program>_objects
            # (cmake/TestRunners.cmake); the program target is its launcher.
            programs.add(target[:-len('_objects')] if target.endswith('_objects') else target)
    listing: dict = json.loads(subprocess.run(['ctest', '--test-dir', str(build), '--show-only=json-v1'],
                                              check=True, capture_output=True, text=True).stdout)
    tests: list[str] = []
    test: dict
    for test in listing.get('tests', []):
        command: list[str] = test.get('command', [])
        names: set[str] = {Path(part).stem for part in command}
        if names & programs:
            tests.append(test['name'])
    print(f'selection: {len(programs)} programs, {len(tests)} tests: {", ".join(sorted(programs))}', flush=True)
    return sorted(programs), tests


def main(argv: list[str] | None = None) -> int:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--platform', default=fetch_test_cache.host_label())
    parser.add_argument('--kind', choices=('normal', 'sanitize'), default='normal')
    parser.add_argument('--archive', type=Path, help='a packed cache on disk (fetch_test_cache.py --archive)')
    parser.add_argument('--unpinned', action='store_true', help='accept an --archive the lock does not name')
    parser.add_argument('--build-dir', type=Path)
    parser.add_argument('--jobs', type=int, default=os.cpu_count() or 4)
    parser.add_argument('--test-jobs', type=int, default=4)
    parser.add_argument('--changed', metavar='BASE', help='build and run only what the changes since BASE select')
    parser.add_argument('--all', action='store_true', help='the whole suite, whatever --changed says')
    parser.add_argument('--tests', metavar='REGEX', help='ctest -R')
    parser.add_argument('--exclude-labels', metavar='REGEX', help='ctest -LE')
    parser.add_argument('--no-test', action='store_true', help='configure and build only')
    arguments: argparse.Namespace = parser.parse_args(argv)
    started: float = time.monotonic()
    suffix: str = '' if arguments.kind == 'normal' else '-' + arguments.kind
    build: Path = (arguments.build_dir or ROOT / '.build' / (arguments.platform + suffix)).resolve()
    try:
        cache: Path = fetch_test_cache.fetch(fetch_test_cache.Lock(fetch_test_cache.LOCK), arguments.platform,
                                             arguments.kind, (ROOT / '.cache' / 'test-cache').resolve(),
                                             arguments.archive, arguments.unpinned)
    except (OSError, ValueError, KeyError) as error:
        print(f'ci_tests: {error}', file=sys.stderr)
        return 1
    print(f'ci_tests: test cache {cache}', flush=True)
    write_file_api_query(build)
    configure: list[str] = ['cmake', '-S', str(ROOT), '-B', str(build), '-G', 'Ninja',
                            '-C', str(cache / 'settings.cmake'), '-DGUI_FORMS_TEST_CACHE=' + cache.as_posix()]
    launcher: str | None = shutil.which('ccache')
    if launcher:
        configure.extend(f'-DCMAKE_{language}_COMPILER_LAUNCHER={launcher}' for language in ('C', 'CXX', 'OBJCXX'))
    try:
        run(configure)
        configured: float = time.monotonic()
        selection: tuple[list[str], list[str]] | None = None
        if arguments.changed and not arguments.all:
            selection = select(build, arguments.changed)
        build_command: list[str] = ['cmake', '--build', str(build), '--parallel', str(arguments.jobs)]
        if selection is not None:
            if not selection[0]:
                print('ci_tests: no test program selected', flush=True)
                return 0
            build_command.extend(['--target', *selection[0]])
        # Ninja keeps going past a failed command (-k 0), so one run reports
        # every error rather than the first (owner, 2026-11-17).
        build_command.extend(['--', '-k', '0'])
        run(build_command)
        built: float = time.monotonic()
        print(f'ci_tests: configure {configured - started:.0f}s, build {built - configured:.0f}s', flush=True)
        if arguments.no_test:
            return 0
        test_command: list[str] = ['ctest', '--test-dir', str(build), '--output-on-failure',
                                   '--parallel', str(arguments.test_jobs), '--output-junit', 'ctest-junit.xml']
        if selection is not None:
            test_command.extend(['-R', '^(' + '|'.join(selection[1]) + ')$'])
        elif arguments.tests:
            test_command.extend(['-R', arguments.tests])
        if arguments.exclude_labels:
            test_command.extend(['-LE', arguments.exclude_labels])
        run(test_command)
        print(f'ci_tests: tests {time.monotonic() - built:.0f}s', flush=True)
    except subprocess.CalledProcessError as error:
        return error.returncode or 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
