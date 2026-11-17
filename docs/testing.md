# Running the tests

This page was the "Tests" section of gui_forms' `docs/building/index.md`
until the tests moved here (2026-11-17). The commands now run in this
repository's build tree (`tools/ci_tests.py` builds it in
`.build/<platform>`).


`ctest` runs the renderer-free core and control tests, the headless host
conformance traces, the painter and text-identity tests, the boundary audits
(no Skia, AppKit, Win32 or X11 type in a public header; no GPU symbol in any
Skia archive) and, when a native host is enabled, that platform's host tests.
The text-identity test compares rendered text against checked-in hashes; it is
the regression check for the covered corpus and rendering profile, not an
exhaustive proof for all text.

Each fuzz target in `fuzz/` also runs in the suite as a `*_fuzz_tests`
program: its seed corpus (from `fuzz/corpus/`, stx's RTF corpus or a
reference file) and a fixed number of deterministic mutations of it
(`fuzz/fuzz_standalone.hpp`). `GUI_FORMS_BUILD_FUZZERS` builds the same
targets for libFuzzer (`cmake/FuzzTargets.cmake`).

Native host tests that open real windows run in CI on each platform. On a
development machine without a display, configure with the host option off
and the renderer-free tests still cover the retained engine.

Tests that open native windows which take focus, capture or activation
carry one of two labels, by what they need from the desktop:

- `foreground`: any desktop of their own. On a development machine they run
  on a private desktop of the same window station, where nothing is ever
  foreground, so they never take the user's foreground and may run in
  parallel, without a lock.
- `input_desktop`: the desktop the user sees. These are the UI Automation
  and MSAA client tests (off the input desktop every UI Automation pattern
  call waits about 2 s inside UIAutomationCore) and the compositor probe (DWM
  does not composite another desktop). They carry the `desktop_foreground`
  resource lock, so one ctest run never runs two at once; across ctest runs
  on one machine, run them under a machine-wide lock.

```text
python -B <private-desktop tool> -- ctest --test-dir build -L foreground
python -B <lock tool> -- ctest --test-dir build -L input_desktop
ctest --test-dir build -LE "foreground|input_desktop"
```

Or set a launcher per group, as a CMake list (CMake 3.29 or later), and a
plain `ctest` runs each test through its group's launcher:
`GUI_FORMS_FOREGROUND_LAUNCHER` (the private-desktop tool) and
`GUI_FORMS_INPUT_DESKTOP_LAUNCHER` (the lock tool). Each is a cache variable;
when the cache value is empty, the environment variable of the same name is
read at every configure. CI leaves both empty: each runner has a desktop of
its own.

A test that reads a service only the input desktop has (the cursor
position, the foreground window, screen pixels) accepts that service's
documented refusal only when it is not on the input desktop
(`tests/support/windows_desktop.hpp`).

