#!/usr/bin/env python3
"""Move gui_forms.lock to a gui_forms commit and copy its test caches' digests.

    python3 tools/update_lock.py <commit>

Reads the .sha256 assets of gui_forms' prerelease test-cache-<commit> (public,
no token needed) and rewrites gui_forms.lock with that commit and one digest
per published platform. A commit with no published cache, or a release with
no .sha256 asset, leaves the lock as it was.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
import urllib.request

import fetch_test_cache

ROOT: Path = Path(__file__).resolve().parents[1]
DIGEST_ASSET: re.Pattern[str] = re.compile(r'^gui_forms_test_cache-([a-z0-9-]+)\.tar\.gz\.sha256$')


def fetch_json(url: str) -> dict:
    """GET url and decode its JSON."""
    request: urllib.request.Request = urllib.request.Request(url, headers={'User-Agent': 'gui-forms-tests',
                                                                           'Accept': 'application/vnd.github+json'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return json.loads(response.read().decode('utf-8'))


def fetch_text(url: str) -> str:
    """GET url as text."""
    request: urllib.request.Request = urllib.request.Request(url, headers={'User-Agent': 'gui-forms-tests'})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read().decode('utf-8')


def main(argv: list[str] | None = None) -> int:
    parser: argparse.ArgumentParser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('commit', help='the full gui_forms commit')
    arguments: argparse.Namespace = parser.parse_args(argv)
    commit: str = arguments.commit
    if not fetch_test_cache.COMMIT_PATTERN.match(commit):
        print('update_lock: give the full 40-digit commit', file=sys.stderr)
        return 1
    lock: fetch_test_cache.Lock = fetch_test_cache.Lock(fetch_test_cache.LOCK)
    release: dict = fetch_json(f'https://api.github.com/repos/{lock.repository}/releases/tags/test-cache-{commit}')
    digests: dict[str, str] = {}
    asset: dict
    for asset in release.get('assets', []):
        match: re.Match[str] | None = DIGEST_ASSET.match(asset['name'])
        if match is None:
            continue
        digests[match.group(1)] = fetch_text(asset['browser_download_url']).split()[0]
    if not digests:
        print(f'update_lock: test-cache-{commit} has no .sha256 asset', file=sys.stderr)
        return 1
    text: str = fetch_test_cache.LOCK.read_text(encoding='utf-8')
    head: str = text.split('[gui_forms]')[0]
    lines: list[str] = [head.rstrip('\n'), '[gui_forms]', f'repository = "{lock.repository}"',
                        f'commit = "{commit}"', '', '[digests]']
    key: str
    for key in sorted(digests):
        lines.append(f'{key} = "{digests[key]}"')
    fetch_test_cache.LOCK.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    print(f'update_lock: gui_forms.lock names {commit} with {", ".join(sorted(digests))}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
