# GUI.Forms tests

The tests of [GUI.Forms](https://github.com/malkuth-software/gui_forms), kept
apart from the library (owner, 2026-10-29 and 2026-11-17) so that a test edit
recompiles that test and nothing of the toolkit.

The tests never compile GUI.Forms. They build against a **test cache**: one
archive per gui_forms commit and platform, packed by gui_forms'
`tools/test_cache.py` and published by gui_forms CI to the prerelease
`test-cache-<commit>`. It holds the installed SDK, the gui_forms tree at that
commit (public and private headers, assets, the CMake modules and tools the
tests call), every library, object library and program the tree built, and a
loader that defines each of them as an imported target of the same name.
[`gui_forms.lock`](gui_forms.lock) names the commit and each archive's SHA-256.

## Running

Prerequisites are those of gui_forms (CMake 3.25+, Ninja, LLVM 22.1 with the
static runtime `GUI_FORMS_LLVM_RUNTIME` names, Python 3.12+). CI and a local
run use the same entry point:

```sh
python3 tools/ci_tests.py                    # fetch the pinned cache, build and run everything
python3 tools/ci_tests.py --changed origin/main   # only the test programs a change touched
python3 tools/ci_tests.py --tests 'gui_forms_png_.*'
```

Against a gui_forms tree of your own (a change not yet published):

```sh
# in gui_forms
python3 tools/ci_build.py --test-cache ../caches
# here
python3 tools/ci_tests.py --archive ../caches/gui_forms_test_cache-<platform>.tar.gz --unpinned
```

`--kind sanitize` runs against the sanitize cache (`ci_build.py --sanitize`).

## Moving the pin

```sh
python3 tools/update_lock.py <gui_forms commit>
```

copies the published digests into `gui_forms.lock`. A pin moves when a
gui_forms change the tests need has been published; a test that needs a
library change lands after it.

## Layout

- `tests/`: every test source, fixture and golden. The history of this
  repository is gui_forms' history of the files that moved here (filtered at
  gui_forms 2b60b1451), followed by the move.
- `CMakeLists.txt` and `cmake/sections/`: the test programs and `add_test`
  blocks, cut from gui_forms' CMakeLists.txt and modules in their order.
- `cmake/TestRuntime.cmake`, `TestRunners.cmake`, `TestPrecompiledHeader.cmake`,
  `TestAreas.cmake`: the shared test runtime, the runner programs, the shared
  precompiled headers and the area labels.
- `tools/`: `ci_tests.py`, `fetch_test_cache.py`, `update_lock.py`, the engine
  gate, the sanitizer suppressions and the test-only checks and labs.
- `benchmarks/` and `fuzz/` (from gui_forms on 2026-11-22): the measurement
  harnesses and probes, none a CTest entry (`cmake/sections/benchmarks.cmake`),
  and the fuzz targets with their seed corpora: the suite's standalone runs
  and the libFuzzer programs (`cmake/FuzzTargets.cmake`,
  `cmake/sections/fuzzers.cmake`, `.github/workflows/fuzz-smoke.yml`). The
  Gallery workloads that gui_forms' own profiling jobs build stay there.
- `tools/font_metrics_differential/` (stage F's differential, library and
  program), the menu, popup-margin and contention drivers, the PNG admission
  probe, the managed facade laboratories and smoke programs.
