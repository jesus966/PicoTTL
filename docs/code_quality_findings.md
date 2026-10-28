# Static-analysis diagnostic triage

## Current clang-tidy status

With clang-tidy 18.1.3, the unchanged `.clang-tidy` configuration and the
existing `tools/quality.py` production manifest, all nine translation units
complete with **zero raw diagnostics**, not just zero new baseline findings.
The clang-tidy baseline list is empty. This is a scoped analysis result,
not proof that every possible static-analysis problem is absent.

The analyzed set is AnsiTerminal, DisplaySurface, Graphics, Terminal, Text,
HostDisplayDevice and the MDA/CGA/EGA device implementations, plus matching
public-header diagnostics. Tests, generated font datasets and the engine's
SDK-recorder copies are outside the CI report manifest; that scope was not
changed to obtain this result.

## Corrected findings

| Check | Initial location / symbol | Cause and correction |
|---|---|---|
| bugprone-branch-clone | VideoMode.hpp, modeHeight() | EGA and MDA/default both return 350. Grouped case labels retain all catalog and fallback results and constexpr evaluation. |
| clang-analyzer-core.UndefinedBinaryOperatorResult | CgaDisplayDevice.cpp, begin() | Capability validation already limits depth to 1/2/4, but analysis did not carry that bound into colorPins indexing. An explicit local array-derived bound makes the physical RGBI limit independently enforced; no array expansion or warning suppression. |
| bugprone-implicit-widening-of-multiplication-result | Engine/GPIO/Text tests | Calculations widened only after multiplication. Widened operands first; Text scrolling offsets now derive from the actual saved-row size. Tested values and expected results are unchanged. |
| clang-analyzer-core.NonNullParamChecker | test_code_page.cpp, font dataset comparison | glyph() can return an empty view. Both pointers are asserted nonnull and memcmp is executed only when both are valid, so a bad resource fails assertions without calling memcmp on null. |

The production CI inventory initially contained five occurrences of two
unique diagnostics. An additional run across all 37 host compilation units
found 26 occurrences and 13 unique diagnostic identities: ten safely
correctable diagnostics (nine source locations; two arguments at one memcmp)
and three intentional negative-test diagnostics. The complete-host run after
the corrections contains only the three diagnostics described below.

The obsolete VideoTiming widening entry was also removed from the baseline:
its production arithmetic and zero-result contract had already been fixed.
No timing calculation was changed by this cleanup. The two live production
entries were removed only after a zero-diagnostic run with the old baseline.

## Deliberate invalid-enum tests outside the production manifest

An additional run across all host translation units under the same
configuration retains three
`clang-analyzer-optin.core.EnumCastOutOfRange` diagnostics:

- test_engine_configuration.cpp constructs SyncPolarity(2) to test rejection.
- test_framebuffer_lifecycle.cpp constructs VideoMode(255) in two rejection paths.

Both enums have fixed uint8 underlying types. Those integer values are
representable in C++, but are intentionally outside PicoTTL's named/supported
values. The checker compares casts with enumerator values, so these are
expected diagnostics on the test inputs, not demonstrated production bugs.
See the [checker implementation](https://github.com/llvm/llvm-project/blob/llvmorg-18.1.3/clang/lib/StaticAnalyzer/Checkers/EnumCastOutOfRangeChecker.cpp)
and [C++17 enumeration value rules](https://timsong-cpp.github.io/cppwp/std17/dcl.enum#8).

The tests and diagnostics remain visible. They have not been suppressed,
added to the baseline, removed from tests or replaced with opaque inputs to
hide the values from analysis. A whole-host analysis therefore reports three
diagnostics; it must not be described as globally warning-free.

## Timing-table initialization and cppcheck status

cppcheck 2.13.0 originally reported three `uninitMemberVar` diagnostics for
MDA/CGA/EGA timing tables. All three tables now have explicit member value
initialization (`{}`). The production analysis has zero raw cppcheck
diagnostics and both analyzer baseline lists are empty; no checks or report
scope were changed.

The tables are embedded `std::uint32_t` arrays, declared before the engine
that borrows their addresses:

| Backend | Capacity (words) | Capacity (bytes) |
|---|---:|---:|
| MDA | 1,480 | 5,920 |
| CGA | 1,048 | 4,192 |
| EGA | 1,456 | 5,824 |

The engine constructor only copies configuration. `buildTimingBuffer()`
validates capacity and segments, then emits every used word before DMA
setup. DMA reads exactly `wordsUsed_`, not the full backend capacity; spare
entries are not consumed. Configuration stops owned hardware first and
invalidates configuration; failure cannot make an incomplete table startable.
Explicit initialization is therefore deterministic initial storage, not a
repair for a demonstrated uninitialized DMA read. Table generation and all
DMA setup/restart operations remain unchanged.

An external before/after object compilation used the Pico 2 Release compile
commands (Arm GCC 15.2.1, SDK 2.3.0, `-O3`, no LTO), adding `-fstack-usage`
for both variants. Initialization adds one constructor-time `memset` of the
listed capacity. Constructor text changes from 280 to 288 bytes (MDA),
124 to 136 (CGA), and 128 to 140 (EGA). All six constructor frames are 56
bytes; this is per-function compiler evidence, not a bound including callees.
There is no temporary table, additional object storage or static RAM. Linked
firmware size and execution cycles cannot be inferred exactly from these
object-section measurements. Static-duration objects are already zeroed by
startup, so the added constructor clear can be redundant; it is bounded and
occurs outside scanout, once per object construction, not on each begin().

Existing engine tests check exact timing words, maximum-component table
population, exact/one-word-short capacities with sentinels, rejected startup,
retry, and DMA counts/restart. These exercise the table-before-DMA contract;
no private-state test hook or duplicate test was added just to observe zeros.
See [baseline comparison/update procedure](code_quality.md#static-analysis-baseline).

Raw reports, unique identities, infrastructure failures and missing baseline
entries must remain distinguishable. Static analysis and host tests do not
validate physical PIO/DMA timing. The maintainer-reported PicoTTL 1.0.0 Arm
hardware tests are historical evidence, not physical validation of this
cleanup or the current development branch.
