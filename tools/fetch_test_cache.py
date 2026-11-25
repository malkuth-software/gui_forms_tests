#!/usr/bin/env python3
"""Fetch, check and unpack the GUI.Forms test cache that gui_forms.lock names.

The cache is one archive per gui_forms commit and platform (gui_forms
tools/test_cache.py packs it; gui_forms CI publishes it to the prerelease
test-cache-<commit>):

    gui_forms_test_cache-<platform>[-<kind>].tar.gz   in release test-cache-<commit>

gui_forms.lock names the commit and each archive's SHA-256. The archive is
downloaded once into the cache root (default .cache/test-cache), checked
against the lock, unpacked beside it into <commit>-<platform>[-sanitize]/,
and reused while its digest matches. The unpacked directory is printed on
the last line of the output.

--archive takes an archive already on disk (a local gui_forms build packed
with test_cache.py pack): its .sha256 beside it must match, and the lock's
digest too unless --unpinned (a cache of a commit the lock does not name
yet, for verifying a gui_forms change before it is published).

A failed fetch or check leaves no unpacked directory; an unpacked directory
is complete (it is renamed into place after unpacking).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform as host_platform
import re
import shutil
import sys
import tarfile
import tempfile
import time
import tomllib
import urllib.request

ROOT: Path = Path(__file__).resolve().parents[1]
LOCK: Path = ROOT / 'gui_forms.lock'
ARCHIVE_ROOT: str = 'gui_forms_test_cache'
COMMIT_PATTERN: re.Pattern[str] = re.compile(r'^[0-9a-f]{40}$')


class Lock:
    """gui_forms.lock: the repository, the commit and the archives' digests."""

    def __init__(self, path: Path) -> None:
        data: dict = tomllib.loads(path.read_text(encoding='utf-8'))
        section: dict = data['gui_forms']
        self.repository: str = section['repository']
        self.commit: str = section['commit']
        if not COMMIT_PATTERN.match(self.commit):
            raise ValueError(f'{path.name}: commit must be 40 hexadecimal digits')
        self.digests: dict[str, str] = dict(data.get('digests', {}))


def host_label() -> str:
    """The platform label of this host, as the caches name it."""
    system: str = host_platform.system().lower()
    machine: str = host_platform.machine().lower()
    arch: str = {'arm64': 'arm64', 'aarch64': 'arm64', 'x86_64': 'x64', 'amd64': 'x64'}.get(machine, machine)
    if system == 'darwin':
        return 'macos-' + arch
    return f'{system}-{arch}'


def archive_name(label: str, kind: str) -> str:
    """The archive's file name (the release names the commit)."""
    suffix: str = '' if kind == 'normal' else '-' + kind
    return f'{ARCHIVE_ROOT}-{label}{suffix}.tar.gz'


def file_digest(path: Path) -> str:
    """The SHA-256 of a file, in hexadecimal."""
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        chunk: bytes = stream.read(1 << 20)
        while chunk:
            digest.update(chunk)
            chunk = stream.read(1 << 20)
    return digest.hexdigest()


def download(url: str, destination: Path) -> None:
    """Downloads url to destination, retrying transient failures three times."""
    attempt: int
    for attempt in range(3):
        try:
            request: urllib.request.Request = urllib.request.Request(url, headers={'User-Agent': 'gui-forms-tests'})
            with urllib.request.urlopen(request, timeout=120) as response, destination.open('wb') as stream:
                shutil.copyfileobj(response, stream, 1 << 20)
            return
        except OSError as error:
            if attempt == 2:
                raise
            print(f'fetch_test_cache: {url}: {error}; retrying', file=sys.stderr, flush=True)
            time.sleep(5 * (attempt + 1))


def unpack(archive: Path, digest: str, destination: Path) -> None:
    """Unpacks archive into destination (renamed into place when complete)
    and records its digest there."""
    parent: Path = destination.parent
    parent.mkdir(parents=True, exist_ok=True)
    work: Path = Path(tempfile.mkdtemp(prefix='unpack-', dir=parent))
    try:
        with tarfile.open(archive, 'r:gz') as tar:
            tar.extractall(work, filter='data')
        unpacked: Path = work / ARCHIVE_ROOT
        if not (unpacked / 'GUIFormsTestCache.cmake').is_file():
            raise ValueError(f'{archive.name} holds no {ARCHIVE_ROOT}/GUIFormsTestCache.cmake')
        # The archive's times are fixed (equal trees, equal archives); stamp
        # the unpacked files now, so a build tree that compiled against an
        # earlier cache sees every header and library as changed and rebuilds
        # what depends on them (ccache keeps the compiles whose text is equal).
        path: Path
        for path in unpacked.rglob('*'):
            if not path.is_symlink():
                os.utime(path)
        (unpacked / '.digest').write_text(digest + '\n', encoding='utf-8')
        if destination.exists():
            shutil.rmtree(destination)
        os.replace(unpacked, destination)
    finally:
        shutil.rmtree(work, ignore_errors=True)


def fetch(lock: Lock, label: str, kind: str, cache_root: Path, archive: Path | None, unpinned: bool) -> Path:
    """The unpacked cache directory, fetched and checked as the module
    docstring says."""
    key: str = label if kind == 'normal' else f'{label}-{kind}'
    pinned: str | None = lock.digests.get(key)
    if archive is not None:
        archive = archive.resolve()
        side: Path = Path(str(archive) + '.sha256')
        digest: str = file_digest(archive)
        if side.is_file() and side.read_text(encoding='utf-8').split()[0] != digest:
            raise ValueError(f'{archive.name} does not match its .sha256')
        if not unpinned and pinned != digest:
            raise ValueError(f'{archive.name} is not the cache gui_forms.lock names (pass --unpinned '
                             'to build against an unpublished cache)')
        commit: str = 'local' if unpinned else lock.commit
    else:
        commit = lock.commit
        if pinned is None:
            raise ValueError(f'gui_forms.lock has no digest for {key}; run tools/update_lock.py')
        digest = pinned
    destination: Path = cache_root / f'{commit}-{key}'
    marker: Path = destination / '.digest'
    if marker.is_file() and marker.read_text(encoding='utf-8').strip() == digest:
        return destination
    if archive is None:
        cache_root.mkdir(parents=True, exist_ok=True)
        name: str = archive_name(label, kind)
        archive = cache_root / name
        if not archive.is_file() or file_digest(archive) != digest:
            url: str = f'https://github.com/{lock.repository}/releases/download/test-cache-{commit}/{name}'
            print(f'fetch_test_cache: downloading {url}', flush=True)
            partial: Path = archive.with_suffix('.partial')
            download(url, partial)
            os.replace(partial, archive)
        if file_digest(archive) != digest:
            raise ValueError(f'{archive.name} does not match gui_forms.lock\'s digest {digest}')
    unpack(archive, digest, destination)
    manifest: dict = json.loads((destination / 'manifest.json').read_text(encoding='utf-8'))
    print(f'fetch_test_cache: {manifest["commit"]} {manifest["platform"]} ({manifest["kind"]}), '
          f'{manifest["targets"]} targets, Clang {manifest["cxx_compiler_version"]}', flush=True)
    return destination


def main(argv: list[str] | None = None) -> int:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('--platform', default=host_label(), help='macos-arm64, linux-x64, windows-x64, ...')
    parser.add_argument('--kind', choices=('normal', 'sanitize', 'tsan', 'profile'), default='normal')
    parser.add_argument('--cache-root', type=Path, default=ROOT / '.cache' / 'test-cache')
    parser.add_argument('--archive', type=Path, help='a packed cache on disk instead of the published one')
    parser.add_argument('--unpinned', action='store_true', help='accept an --archive the lock does not name')
    arguments: argparse.Namespace = parser.parse_args(argv)
    try:
        directory: Path = fetch(Lock(LOCK), arguments.platform, arguments.kind, arguments.cache_root.resolve(),
                                arguments.archive, arguments.unpinned)
    except (OSError, ValueError, KeyError) as error:
        print(f'fetch_test_cache: {error}', file=sys.stderr)
        return 1
    print(directory)
    return 0


if __name__ == '__main__':
    sys.exit(main())
