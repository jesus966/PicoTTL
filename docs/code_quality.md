# Code quality and CI

The workflows define five checks: **Host test suite**, **RP2350 firmware
build**, **Host sanitizers and coverage**, **Host static analysis** and
**CodeQL C++ host**. Their names must remain stable. The initial rollout
policy required the first two and treated new analyses as advisory. Actual
required-check settings live in GitHub rulesets, not these workflow files;
inspect those settings before changing protections. Do not remove a check
that has already been promoted to required.

There is no blanket `continue-on-error`, automatic production fix, warning
suppression, or minimum coverage percentage. Static analysis uses an explicit,
versioned baseline for reviewed pre-existing diagnostics; see below.

The production manifest currently has zero raw clang-tidy 18.1.3 diagnostics
under the existing checks. cppcheck 2.13.0 also reports zero raw diagnostics;
both analyzer baseline lists are empty. The broader
host test sources retain three deliberate invalid-enum test-input diagnostics;
see [diagnostic triage](code_quality_findings.md) for scope and explanations.
Do not interpret production-scope cleanliness as a whole-project guarantee.

## Scope and reproducibility

The host build is a standalone C++17 project in `tests/`. It includes the
portable library, the host device, and the real MDA/CGA/EGA device lifecycle
code linked against `RecordingScanlineEngine`. The latter records calls; it
does not execute PIO or DMA. Engine configuration tests additionally compile
the actual scanline-engine implementation under a test-only class name against
a recording SDK boundary. They exercise returned errors, resource ownership
and restart decisions, but do not execute PIO instructions, DMA transfers or
asynchronous aborts. Pointer addresses stored in 32-bit DMA words are not
dereferenced by that boundary on a 64-bit host. The firmware build remains a
separate Pico SDK
project. Instrumentation options belong only to the host executable and
default to OFF; instrumented cross-compilation is rejected.

All quality jobs use Ubuntu 24.04. Locally verified tools:

| Tool | Version |
|---|---|
| GCC / gcov | 13.3.0 |
| clang-tidy | 18.1.3 |
| cppcheck | 2.13.0 |
| lcov | 2.0 |
| PyYAML (structured clang-tidy report parser) | 6.0.1 |
| CodeQL CLI / cpp queries | 2.27.3 / 1.9.2 |
| actionlint / ShellCheck | 1.7.12 / 0.9.0 |

CI explicitly selects GCC 13 and clang-tidy 18. Ubuntu security/package
updates can change patch versions: tool versions are printed in the logs.
lcov and cppcheck come from the same Ubuntu release. Actions are pinned to
immutable commit SHAs, with release versions alongside them; update these
pins in reviewed maintenance PRs. Current pins are checkout 7.0.1,
upload-artifact 7.0.2 and CodeQL Action 4.39.1. This targets GitHub.com hosted
runners, not old self-hosted runners or GitHub Enterprise Server.

On Ubuntu 24.04 (including WSL), install the local dependencies once:

```sh
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++-13 clang-tidy-18 cppcheck lcov python3-yaml
```

## Host tests, sanitizers and coverage

Ordinary tests:

```sh
cmake -S tests -B build-host -G Ninja -DCMAKE_CXX_COMPILER=g++-13
cmake --build build-host --parallel 2
ctest --test-dir build-host --output-on-failure --verbose --no-tests=error
```

The single CTest registration, `picottl_host_tests`, invokes the existing runner,
which reports each individual case. List registrations with
`ctest --test-dir build-host -N`. The registration count is not the case count.
`--no-tests=error` prevents an empty suite from passing unnoticed.
CTest resolves the built executable and works with out-of-source builds;
the executable can also be run directly for debugging.
For ASan, UBSan and gcov, use a fresh, separate directory:

```sh
cmake -S tests -B build-quality -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=Debug \
  -DPICOTTL_ENABLE_SANITIZERS=ON -DPICOTTL_ENABLE_COVERAGE=ON
cmake --build build-quality --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-quality --output-on-failure --verbose --no-tests=error
mkdir -p reports/coverage
lcov --capture --directory build-quality --gcov-tool gcov-13 \
  --output-file reports/coverage/raw.info
python3 tools/quality.py coverage --input reports/coverage/raw.info \
  --output-dir reports/coverage
genhtml reports/coverage/lcov.info --output-directory reports/coverage/html --legend
```

GCC and gcov must match. Repeated runs accumulate `.gcda` counters; use a
fresh directory or `lcov --zerocounters --directory build-quality` before
measuring another run. Do not interpret accumulated counts as independent
executions. ASan/UBSan and coverage share one CI build and test execution.
Sanitizers require working native runtimes; Linux is the verified platform.

The report allowlist includes only compiled portable production code and
host-executed device lifecycle logic, plus their own headers. It excludes
test code, the recording engine, generated font datasets, third-party code
and the scanline-engine source and its recording SDK, including the engine
logic executed by the configuration tests. Those tests run in the normal
host/CI executable, but the established report scope does not count them as
hardware coverage. Functions and lines instantiated in multiple
translation units are counted once in the summary. No branch-coverage claim
is made. Code not compiled by the host target is outside the denominator;
the percentage must never be presented as whole-firmware coverage.
When adding a production host module, update the allowlist in
`tools/quality.py` and its scope tests so analysis and coverage include it.

Neither a clean sanitizer run nor high coverage establishes physical signal
timing, DMA quiescence, multicore safety or CRT compatibility. Existing
host tests do not exhaust all legal API inputs or lifecycle failures.

## Firmware stack measurements

Use the target compiler rather than host stack reports to budget firmware
stack usage. A separate build keeps measurement artifacts out of normal
builds. For the SDK's Release optimization settings:

```sh
cmake -S . -B build/stack -G Ninja -DPICO_BOARD=pico2 \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_C_FLAGS_RELEASE=-O3 -DNDEBUG -g -fstack-usage' \
  '-DCMAKE_CXX_FLAGS_RELEASE=-O3 -DNDEBUG -g -fstack-usage'
cmake --build build/stack --parallel 2
find build/stack -name '*.su' -print
arm-none-eabi-size build/stack/examples/01_mda_sync/picottl_example_01_mda_sync.elf
arm-none-eabi-nm -S -C build/stack/examples/01_mda_sync/picottl_example_01_mda_sync.elf
```

Keep the normal SDK architecture flags: do not replace `CMAKE_C_FLAGS` or
`CMAKE_CXX_FLAGS` with only `-fstack-usage`. Use the same SDK, compiler,
optimization, assertion and LTO settings when comparing builds. Different
optimization levels can change inlining, register spills and call frames.
With LTO, inspect any link-time `.su` files as well; compile-time reports alone
can omit or misrepresent the final functions. Check final disassembly too.

Each `.su` record describes a function's own compiler-reported frame, not
the accumulated stack of its callers, callees and interrupts. Inlined code
is accounted in the containing frame; out-of-line compiler clones may have
their own entries. Tail calls do not retain the caller's whole frame.
Inspect indirect/virtual calls and include SDK routines, assembly and
precompiled library routines that may have no `.su` record. Add exception
entry and interrupt nesting appropriate to the actual application. A sum
over a selected call path is an estimate, not a complete upper bound.

Inspect the ELF/map symbols `__StackTop`, `__StackBottom`, `__StackOneTop`
and `__StackOneBottom` to determine the actual linker reservations. Pico SDK
2.3.0 defaults `PICO_STACK_SIZE` to 0x800 bytes; `PICO_CORE1_STACK_SIZE`
defaults to that value when a default core-1 stack is linked. An unused core
may have no stack reservation. A custom core-1 launch stack or RTOS task
stack has its own budget. In the default bare-metal Cortex-M33 execution
model, foreground code and interrupts use the per-core main stack; there is
no additional independent interrupt-stack allowance. `__StackLimit` is a
historical heap-boundary symbol and must not be mistaken for a stack bottom.

A linker reservation is not a measured high-water mark or proof that
overflow will fault: SDK stack guards are optional. For runtime validation,
use a debugger or carefully implemented watermark measurement with the
application's interrupt load. Observe each active core separately. Never
overwrite live stack contents to install a watermark.

Moving an application-lifetime object out of an automatic frame exposes its
storage in `.bss` or `.data`; it does not remove the object's RAM requirement.
Compare both ELF section sizes and symbol sizes. Local static initialization
can also link a guard and destructor-registration support; check the map
rather than assuming the RAM/code delta equals only `sizeof(device)`.
See the [memory policy](architecture.md#7-memory-policy) for ownership and
lifetime constraints. Static storage does not establish reentrancy or multicore
safety, and stack reports do not validate physical video signals.

## Static analysis

Configure once; both analyzers reuse the same compilation database. A
firmware compilation database containing the real scanline engine is
deliberately rejected, and is not the supported input.
Use the host database below, not an ARM/Pico SDK build:

```sh
cmake -S tests -B build-static -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=Debug
python3 -m unittest discover -s tools/quality_tests -v
python3 tools/quality.py static clang-tidy --executable clang-tidy-18 \
  --build-dir build-static --output-dir reports/clang-tidy
python3 tools/quality.py static cppcheck \
  --build-dir build-static --output-dir reports/cppcheck
```

Run each command even if the other analyzer reports findings. In CI the
second analyzer and publication steps explicitly run after a first-tool
failure, provided configuration succeeded. Generated font translation
units and the hardware scanline engine are excluded before analysis;
external/system headers are not reported as project clang-tidy diagnostics.

clang-tidy enables analyzer, bugprone, performance and portability checks.
Only the easily-swappable-parameters heuristic is disabled: coordinate and
color arguments intentionally share types in the frozen API. Other findings
remain visible, including possible false positives. No `--fix` is used.
cppcheck enables warning, performance and portability diagnostics.

Each analyzer writes a complete log, machine-readable report and summary:

- Exit 0 / `clean` or `known_findings`: completed with no new diagnostics.
- Exit 1 / `new_findings`: at least one diagnostic outside the baseline.
- Exit 2 / `infrastructure_error`: missing executable, bad database,
  incomplete XML, parsing/compiler configuration failure or tool crash.

Counts distinguish occurrences from unique diagnostics: a header may be
reported in several translation units. All raw logs, YAML/XML and normalized
`diagnostics.json` remain in the artifacts, including known warnings. The
summary lists known, new and missing IDs; `result.json` contains the complete
comparison. `invocations.json` records source scope and native exit codes.
Findings are not automatically called defects; use the
[initial triage](code_quality_findings.md) and separate `bugfix/` branches.

### Static-analysis baseline

[`tools/quality-baseline.json`](../tools/quality-baseline.json) now has empty
clang-tidy and cppcheck lists after the corresponding raw
diagnostics disappeared. Retired entries are described in
[diagnostic triage](code_quality_findings.md). The baseline changes only the
wrapper's acceptance decision:
native warning/error diagnostics are neither disabled nor removed. Known-only
runs are green; any new identity fails. Disappeared entries are reported for
cleanup but do not fail an otherwise complete run. No GitHub permissions or
required-check names change.

Identities use exact tool/rule, relative file path, message, severity, lexical
source context, enclosing brace scopes and occurrence within identical code.
Source comments, inter-token whitespace, absolute checkout paths, line numbers
and byte offsets are not identity fields. A separate digest covers anchored
analysis notes/paths or cppcheck symbols and details, so different traces at
the same expression do not silently match. Duplicate reports of the same
diagnostic across translation units share an identity and remain as separate
occurrences in the artifact.

The source anchor is conservative, not a C++ semantic parser. Structural code
changes, changed messages/traces or a tool update can legitimately produce new
IDs that need review. Unmappable or ambiguous report locations, malformed
reports, compiler failures and crashes fail as infrastructure errors; there
is no fallback to file/rule-only matching. All expected host translation units
must be present. clang-tidy YAML must match the current unit and displayed
diagnostic count; stale YAML is removed before invocation. A clean clang-tidy
unit normally produces no YAML. cppcheck must report completed file progress
and valid XML. No disappeared-entry comparison is claimed for incomplete runs.

To update the baseline after fixing a defect:

1. Run both analyzers with the normal commands above. Inspect the full logs
   and `result.json`; verify that analysis completed and no unexpected new
   diagnostics appeared.
2. Review `baseline.missing` / the **missing** summary entries and remove only
   the corresponding entries from the versioned baseline. Do not regenerate
   the baseline wholesale or automatically accept all current output.
3. If a code/tool change shifted an unchanged diagnostic's identity, inspect
   both its old entry and the new `diagnostics.json` record, including the raw
   trace. A reviewed update copies that record's exact `id` and `identity`
   and retains an explicit `reason`. New defects require a separate triage
   decision; a green job alone is not grounds to add them.
4. Run the infrastructure tests and both analyzers again. Confirm expected
   known/new/missing counts, review the baseline diff, and include the narrow
   baseline removal/update with its bugfix or tool-maintenance PR.

For an isolated review, `--baseline path/to/candidate.json` selects another
baseline without overwriting the repository file. Schema, duplicate keys,
duplicate IDs and identity hashes are validated. Missing tools, truncated or
unusable reports must never be turned into baseline-accepted results.

## CodeQL

The manual C/C++ build traces the same native host target. Root-level
autobuild would select the Pico SDK cross-build and is intentionally not
used. A fresh traced build is necessary; objects from another job cannot
substitute for extraction. Local database creation and finalization have
been verified with GCC 13, including the host-executed device sources.

Install the official CodeQL 2.27.3 C++ bundle, verify its published checksum,
and put its `codeql` directory on PATH. Follow CodeQL's licensing conditions
for local use. Then, from the repository root:

```sh
cmake -S tests -B build-codeql -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=Debug
codeql database create ../picottl-codeql-db --language=cpp \
  --source-root=. --command='cmake --build build-codeql --parallel 2'
codeql database analyze ../picottl-codeql-db \
  codeql/cpp-queries:codeql-suites/cpp-security-extended.qls \
  --format=sarif-latest --output=codeql-raw.sarif --threads=2 --ram=2048
python3 tools/quality.py sarif --input codeql-raw.sarif --output-dir reports/codeql
```

Both the full SARIF and the scoped production SARIF are retained in CI.
Only the latter is uploaded to code scanning. Extraction includes tests
and generated constant font data needed for a valid build; report filtering
excludes results whose primary location is outside own host production code.
Unresolved/locationless diagnostics are retained. `paths-ignore` is not used
as a misleading exclusion for manually compiled C++. No query IDs are
globally suppressed. Host extraction includes the actual engine control flow
compiled under test-only names against the recorder; the published report
scope excludes these engine copies. Real SDK/MMIO execution and the firmware
examples/applications are not analyzed by this host CodeQL configuration.

## PR reports, permissions and failure policy

Coverage HTML, LCOV, JSON and CTest logs are retained for 14 days in the
`host-sanitizers-coverage` artifact. Static reports and raw/scoped CodeQL
SARIF have separate artifacts with the same retention. Coverage and static
summaries appear under the workflow run Summary, reachable from the PR's
Checks tab and the corresponding check's Details link. Download the coverage
artifact and open `html/index.html` for per-file detail.

This does not create conversation comments or upload coverage to an
external service. The report jobs are designed for fork PRs without PR
write access; execution may first require maintainer approval under the
repository's fork-workflow policy. Public fork PRs receive a read-only
token and no repository secrets. Keep the settings that send write tokens
or secrets to fork workflows disabled.
No `pull_request_target` or privileged `workflow_run` is used by these
checks. Their workflows use `contents: read`; only the CodeQL job requests
`security-events: write`. Release/sync automation has separate permissions.
Fork tokens/code-scanning availability are governed by GitHub. Upload
failures are infrastructure failures; SARIF artifacts remain available.
The current repository is public. A future private deployment also needs
the appropriate Code Security license and `actions: read` on the CodeQL
job for SARIF upload; this additional permission is not granted here.

Sanitizer failures retain CTest logs; CI does not publish a coverage number
when the test run fails. Report-generation and upload failures remain
visible failures. CodeQL alerts appear in code scanning; a successful
CodeQL job does not mean there are no alerts. Do not enable a code-scanning
merge-protection rule during the initial advisory phase.

The host, firmware, quality and CodeQL workflows have unfiltered
`pull_request` triggers; their push triggers select `main`, `develop` and
`release/**`. Quality and CodeQL also support manual dispatch. Topic branches
are normally checked through PRs, avoiding simultaneous push and PR jobs.

## GitHub setup and promotion to required checks

The following is rollout guidance, not a statement of the live ruleset.
Retain protections already in force. Run the workflows so GitHub can
discover the check names:

1. In **Settings → Advanced Security → Code Security → CodeQL analysis**,
   select **Set up → Advanced** for this repository workflow. If default
   setup is active, choose **Switch to advanced → Disable CodeQL** first.
   Keep the reviewed workflow from this branch rather than committing a
   second generated template. Public repositories support code scanning;
   private organization repositories need GitHub Code Security enabled.
2. In **Settings → Rules → Rulesets**, create or edit an active branch
   ruleset targeting `main`, `develop` and the intended `release/**` branches.
   Retain existing protections and bypass policy. Require PRs and require
   status checks to pass before merging.
3. Retain **Host test suite** and **RP2350 firmware build**, using
   **GitHub Actions** as the expected source. Keep these required. GitHub
   only offers eligible checks that succeeded in the past seven days;
   open a PR/run CI first if absent.
4. During initial rollout only, leave **Host sanitizers and coverage**, **Host static analysis**
   and **CodeQL C++ host** unselected. Do not add required code-scanning
   severity rules yet. If using classic branch protection, apply the same
   check names there and inspect overlapping rulesets. Do not unselect any
   analysis check already required by the project.
5. After representative PRs and fork runs are stable, first promote host
   sanitizers, then CodeQL execution. Independently decide which CodeQL
   alert severities should block. Promote static analysis after triaging
   its existing diagnostics and eliminating false positives with narrowly
   justified changes. Coverage can remain informational without a threshold.

If **Require branches to be up to date** is enabled, check the merge result
through normal PR CI. Do not enable merge queue without also adding and
validating `merge_group` triggers for every required workflow.

Planning estimates on GitHub-hosted runners, not measured Actions timings:
ordinary host CI 1–2 minutes, firmware 3–8 minutes, sanitizers/coverage 1–3
minutes, both static tools 1–3 minutes, CodeQL 3–8 minutes. Jobs run in
parallel. No shared object cache hides CodeQL extraction; only analysis
databases and reports that are actually produced are claimed as validated.
