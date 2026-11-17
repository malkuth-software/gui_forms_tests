# GUI.Forms tests: guardrails

This repository holds GUI.Forms' tests (owner, 2026-10-29 and 2026-11-17).
It never compiles GUI.Forms: it builds against the test cache that
`gui_forms.lock` names (gui_forms `tools/test_cache.py`; see README.md).
The library, its build, packaging and smoke check stay in
malkuth-software/gui_forms, whose AGENTS.md governs them.

- A test that needs a library change lands after that change is published
  as a test cache; move the pin with `python3 tools/update_lock.py <commit>`.
  Never patch, copy or vendor library sources here to get ahead of the pin.
- A few tests compile library sources under test-only definitions (the
  Windows DIB and prepared-text host variants, the offscreen raster
  factory, the surface and web host rules): they compile them from the
  cache's `source/` tree, never from a copy.
- Verify like CI: `python3 tools/ci_tests.py` is the entry point locally and
  in CI, with the same inputs (`--archive ... --unpinned` for a gui_forms tree
  of your own). Build and run on the M4, never on a laptop.
- Warnings count as failures where gui_forms counts them; ninja keeps going
  past a failure (`-k 0`) so one run lists every error.

## Code style (mandatory)

All first-party C++ and Objective-C++ in this repository follows the house
style. Its canonical text is `HOUSE_STYLE.md` on the `main` branch of
`malkuth-software/notes`
(https://github.com/malkuth-software/notes/blob/main/HOUSE_STYLE.md). Read it
in full before writing or changing code. One line per section, in its order
(most important first, the owner's 2026-10-27 ranking), so a skim still covers
all eleven:

1. **Memory use:** every datum has an owner and a lifetime; allocate before
   repeated processing and reuse; transfer ownership explicitly; raw
   addresses only at boundaries; name storage operations (fill, reserve,
   reset, clear, release) precisely; check size arithmetic; keep borrows
   valid through use (a function returning a borrow of an argument deletes
   its rvalue overload; declare a borrower after what it borrows); memory
   errors and undefined behaviour are defects in tests too, checked by
   sanitizer runs of the whole suite.
2. **Failure and foreign interfaces:** say what a failure leaves behind;
   validate before mutating; explicit results for expected alternatives and
   exceptions only at subsystem boundaries; switches and state machines keep
   a default that reaches one fallback; foreign interfaces state types,
   counts, ownership and errors, and take entry-point types from their
   declarations, never retyped by hand.
3. **Types and values:** spell types; separate confusable quantities (byte
   offsets and character positions, identities and indices) with wrapper
   types; one numeric precision per computation; named record fields, enums
   and explicit initialization; state array shape and capacity versus live
   length.
4. **Objects and authority:** parents own children and back links only
   observe; cross authority boundaries through named capabilities; define
   the fallback when an owner disappears (reject stale identities, stop
   revoked callbacks).
5. **Callbacks and execution:** named targets with explicit context; specify
   event order, reentrancy and revocation; complete a state change before
   notifying, and carry nothing taken from state across a call into
   application code; name asynchronous work, completion, cancellation and
   shutdown; one execution context per piece of mutable state.
6. **Procedures and data:** state inputs, outputs and permitted mutation;
   fill caller-supplied storage for bulk work; name intermediate forms; state
   overlap rules.
7. **Admitted abstractions:** standard algorithms with named predicates;
   templates for declared choices; virtual dispatch and type erasure only
   where runtime variation is required; no universal callable frameworks or
   speculative portability layers.
8. **Repeated kernels:** no decisions inside the loop: a per-element branch
   marks an unfinished formulation, so find the reformulation rather than
   evaluating both alternatives; break the local paradigm, then restore
   exactness with corrections outside the interior, proved against a kept
   reference; bookkeeping lives outside the loop; then layout, hoisting and
   stated numerical freedoms.
9. **Statements and expressions:** write in execution order; no lambdas or
   anonymous callbacks; one recognizable operation per expression; calculate,
   then return; break chains that hide types, state or allocation.
10. **Source layout and comments:** meaningful source boundaries with the
    main sequence easy to find; no accessors or wrappers that add no
    invariant; explain non-obvious dimensions, lifetimes and surprising
    operations; remove stale comments.
11. **C++ spelling:** no `auto`, trailing returns or structured bindings;
    `(*pointer).member` instead of `->`; no lambdas; hand-written comparisons
    (no `= default`); no coroutines or ranges pipelines.

Match the surrounding code's naming and comment density.

Before committing, run the checks the `house-style` job runs, with
gui_forms' tools from the cache's source tree (`<cache>` is the directory
`tools/fetch_test_cache.py` prints; the tools need template_harness at
`<cache>/template_harness`, at `<cache>/source/harness.lock`'s commit):

```text
python3 -B <cache>/source/tools/check_house_style.py tests file_manager_demoboard/tests tools
python3 -B <cache>/source/tools/check_lifetime_rules.py tests file_manager_demoboard/tests tools
python3 -I <cache>/source/tools/check_std_rules.py --config tools/std_rules.toml
python3 -B tools/check_test_registration.py --root .
python3 -B tools/check_test_asserts.py --root .
python3 -B <cache>/source/tools/abi_class_tables.py --check --tests-root .
python3 -B <cache>/source/tools/generate_library_docs.py --check --tests-root .
```

The last two check the files gui_forms' generators write here (the C# hook
test, the documentation examples) against the pinned tree; regenerate them
with the same tools without `--check`.

## Layout

- `tests/`: the test sources, fixtures, goldens and consumer projects.
- `CMakeLists.txt`: loads the cache (`GUIFormsTestCache.cmake`), then the
  sections in `cmake/sections/`, cut from gui_forms in their original order.
- `cmake/`: the test runtime, the shared runners, the shared precompiled
  headers, the area labels and the test-only CMake scripts.
- `tools/`: `ci_tests.py`, `fetch_test_cache.py`, `update_lock.py`, the
  engine gate, the layout oracle, the sanitizer suppressions, the test-only
  checks and labs.
