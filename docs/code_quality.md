# Code quality and CI

The two existing required checks keep their exact names: **Host test suite**
and **RP2350 firmware build**. New checks are advisory initially: their real
failures remain visible, but repository rules must not require them yet.
There is no blanket `continue-on-error`, automatic production fix, warning
baseline suppression, or minimum coverage percentage.

## Scope and reproducibility

The host build is a standalone C++17 project in `tests/`. It includes the
portable library, the host device, and the real MDA/CGA/EGA device lifecycle
code linked against `RecordingScanlineEngine`. The latter records calls; it
does not execute PIO or DMA. The firmware build remains a separate Pico SDK
project. Instrumentation options belong only to the host executable and
default to OFF; instrumented cross-compilation is rejected.

All quality jobs use Ubuntu 24.04. Locally verified tools:

| Tool | Version |
|---|---|
| GCC / gcov | 13.3.0 |
| clang-tidy | 18.1.3 |
| cppcheck | 2.13.0 |
| lcov | 2.0 |
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
sudo apt-get install -y cmake ninja-build g++-13 clang-tidy-18 cppcheck lcov
```

## Host tests, sanitizers and coverage

Ordinary tests:

```sh
cmake -S tests -B build-host -G Ninja -DCMAKE_CXX_COMPILER=g++-13
cmake --build build-host --parallel 2
ctest --test-dir build-host --output-on-failure --verbose
```

The CTest entry invokes the existing runner, which reports all 68 cases.
For ASan, UBSan and gcov, use a fresh, separate directory:

```sh
cmake -S tests -B build-quality -G Ninja \
  -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_BUILD_TYPE=Debug \
  -DPICOTTL_ENABLE_SANITIZERS=ON -DPICOTTL_ENABLE_COVERAGE=ON
cmake --build build-quality --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-quality --output-on-failure --verbose
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
and the real scanline engine. Functions and lines instantiated in multiple
translation units are counted once in the summary. No branch-coverage claim
is made. Code not compiled by the host target is outside the denominator;
the percentage must never be presented as whole-firmware coverage.
When adding a production host module, update the allowlist in
`tools/quality.py` and its scope tests so analysis and coverage include it.

Neither a clean sanitizer run nor high coverage establishes physical signal
timing, DMA quiescence, multicore safety or CRT compatibility. Existing
host tests do not exhaust all legal API inputs or lifecycle failures.

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

- Exit 0 / `clean`: completed without reported findings.
- Exit 1 / `findings`: analyzer diagnostics, requiring review.
- Exit 2 / `infrastructure_error`: missing executable, bad database,
  incomplete XML, parsing/compiler configuration failure or tool crash.

Counts are diagnostic occurrences; a header may be reported in several
translation units. Findings are not automatically called defects. Compare
with [the initial triage](code_quality_findings.md), reproduce suspected
defects, and use separate `bugfix/` branches. An existing warning remains
visible and can keep the advisory static-analysis job red. Missing tools
or unusable reports must not be treated as a successful analysis.

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
globally suppressed. The real PIO/DMA engine, examples and applications
are compiled by firmware CI but are not CodeQL-analyzed in this first phase.

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
No `pull_request_target` or privileged `workflow_run` is used. Workflows
use `contents: read`; only the CodeQL job requests `security-events: write`.
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

Workflows run for PRs and pushes to `main`, `develop` and `release/**`, plus
manual dispatch for the new quality workflows. Topic branches are checked
through PRs, avoiding simultaneous push and PR jobs for the same topic.

## GitHub setup and promotion to required checks

No repository settings are changed by this branch. After reviewing/merging
the workflows, run them once so GitHub can discover the check names:

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
3. Select exactly **Host test suite** and **RP2350 firmware build**, using
   **GitHub Actions** as the expected source. Keep these required. GitHub
   only offers eligible checks that succeeded in the past seven days;
   open a PR/run CI first if absent.
4. Initially leave **Host sanitizers and coverage**, **Host static analysis**
   and **CodeQL C++ host** unselected. Do not add required code-scanning
   severity rules yet. If using classic branch protection, apply the same
   two check names there and inspect overlapping rulesets.
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
