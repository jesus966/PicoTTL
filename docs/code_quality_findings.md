# Static-analysis diagnostic triage

This page explains reviewed diagnostics tracked in
[the baseline](../tools/quality-baseline.json). Reports retain all findings;
known diagnostics may pass comparison, new ones fail, and infrastructure or
incomplete-report errors fail separately. Baseline membership is not proof
that a diagnostic is harmless or that a defect has been fixed. See the
[comparison and update procedure](code_quality.md#static-analysis-baseline).

## Corrected timing arithmetic and a stale baseline entry

`VideoTiming::refreshRateMilliHz()` now widens horizontal/vertical totals
before multiplication, guards a zero denominator and checks representability
before returning uint32. Its zero-result contract is documented in
[VideoTiming.hpp](../include/picottl/video/VideoTiming.hpp) and exercised by
[test_video_timing.cpp](../tests/test_video_timing.cpp), including constexpr
cases. The earlier unsigned-product/zero-divisor defect is not outstanding.

The baseline still contains its `bugprone-implicit-widening-of-multiplication-result`
entry. A report should identify it as missing when the warning no longer
appears. Removing an obsolete entry requires the normal reviewed baseline
update; documentation changes do not alter that gate or suppress diagnostics.

## CGA array warning: not established as a defect

clang-tidy's `clang-analyzer-core.UndefinedBinaryOperatorResult` reports a path
to `colorPins[4]` in `CgaDisplayDevice::begin()`. The preceding capability
check admits only 1/2/4-bit formats, making that path infeasible under the
current implementation. Treat it as a likely interprocedural false positive,
not demonstrated out-of-bounds access. Preserve the report pending a narrow
analysis decision; do not suppress all core analyzer warnings.

## Intentional repeated mode height

`bugprone-branch-clone` identifies identical EGA350 and MDA/default results in
`modeHeight()` in [VideoMode.hpp](../include/picottl/VideoMode.hpp). Both are
350 under the current catalog. This is a maintainability diagnostic, not a
runtime defect.

## Device timing-table initialization warnings

cppcheck's `uninitMemberVar` identifies the MDA/CGA/EGA devices' embedded
timing tables. Configuration fills the used entries before DMA setup; the
analyzer does not model that ownership/write path. No uninitialized read is
demonstrated on valid startup. Resource/configuration tests exercise this
sequence without executing hardware transfers. Keep these reports visible
until a focused reviewed decision changes the baseline or implementation.

Header diagnostics may repeat across translation units. Compare both raw
occurrences and stable unique identities; do not hard-code historical case
counts or percentages as evidence of the current suite. Host sanitizer and
CodeQL results complement these findings rather than overruling them.
