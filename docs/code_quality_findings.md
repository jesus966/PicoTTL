# Initial quality triage

These findings predate the quality-infrastructure branch. No production
fixes or baseline suppressions are included. This is a triage record, not
an automatically accepted allowlist. Refresh it when tools or scope change.

## Confirmed arithmetic defect: timing frequency denominator

- Tool: clang-tidy 18.1.3,
  `bugprone-implicit-widening-of-multiplication-result`.
- Location: `include/picottl/video/VideoTiming.hpp:70`,
  `VideoTiming::refreshRateMilliHz()`.
- Evidence: `hTotal() * vTotal()` is evaluated in 32 bits before division
  into the 64-bit numerator. A descriptor with `hActive=65535`,
  `hFrontPorch=1`, `vActive=65535`, `vFrontPorch=1`, and the other spans
  zero has totals 65536 each; their product wraps to zero, causing an
  integer division by zero. This follows directly from the unsigned types
  and was reproduced in a temporary standalone program with GCC 13 UBSan
  (`VideoTiming.hpp:70:64: runtime error: division by zero`, exit 1).
- Severity: medium, high confidence for custom descriptors. Current
  catalog modes do not approach this limit. This does not establish a
  failure on existing MDA/CGA/EGA timings.
- Follow-up: a separate bugfix should widen before multiplication and
  define invalid-descriptor handling, with UBSan regression inputs.

## CGA array warning: not established as a defect

- Tool: clang static analyzer through clang-tidy,
  `clang-analyzer-core.UndefinedBinaryOperatorResult`.
- Location: `src/rp2350/CgaDisplayDevice.cpp:96`, `begin()`.
- The reported path reaches `colorPins[4]`. The earlier capability check
  admits only 1/2/4-bit formats, making that path infeasible under the
  current capability implementation. Treat as a likely interprocedural
  false positive, not demonstrated out-of-bounds access. Keep the report
  visible pending a narrow analysis/annotation decision; do not alter the
  public API or suppress all core analyzer warnings.

## Intentional repeated mode height

- Tool: clang-tidy, `bugprone-branch-clone`.
- Location: `include/picottl/VideoMode.hpp:78`, `modeHeight()`.
- EGA350 and MDA/default both return 350. This is correct current behavior;
  the diagnostic is a maintainability observation, not a runtime defect.

## Device timing-table initialization warnings

- Tool: cppcheck 2.13.0, `uninitMemberVar`.
- Locations: MDA constructor at `src/rp2350/MdaDisplayDevice.cpp:52`,
  CGA at `src/rp2350/CgaDisplayDevice.cpp:50`, EGA at
  `src/rp2350/EgaDisplayDevice.cpp:55`.
- The table is deliberately filled by engine configuration before DMA
  starts. The analyzer does not model that ownership/write path. There
  is no demonstrated uninitialized read in the valid startup path.
  Retain as pre-existing warnings until that contract is narrowly encoded
  or verified; zero-filling production tables is not part of this branch.

Repeated header diagnostics occur once per analyzed translation unit.
Neither clean sanitizer tests nor an empty CodeQL SARIF dismisses the
arithmetic defect: the current 68 tests do not exercise its extreme input.

The initial native runs produced 9 clang-tidy diagnostic occurrences
(4 repeated-height, 4 timing arithmetic, 1 CGA path warning), 3 cppcheck
warnings, and no infrastructure errors. CodeQL CLI 2.27.3 completed 98
security-extended queries with 0 alerts in both raw and scoped SARIF.
