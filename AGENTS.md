# Instructions for coding agents

## Project and architecture

PicoTTL is a C++17 IBM MDA/CGA/EGA TTL video library for Raspberry Pi Pico 2
(RP2350), using Pico SDK 2.3.0. Read [AI_POLICY.md](AI_POLICY.md) and
[CONTRIBUTING.md](CONTRIBUTING.md); use [architecture.md](docs/architecture.md)
for detailed contracts.

- `include/picottl/` contains public surfaces and mostly header-only packed
  framebuffer storage. `src/` implements portable Graphics, Text, Terminal,
  AnsiTerminal and DisplaySurface; `src/fonts/` contains generated datasets.
- `Display` is the application facade over `DisplayDevice`; `DisplaySurface`
  and `Framebuffer` separate drawing from storage and output.
- `src/rp2350/` implements MdaDisplayDevice, CgaDisplayDevice,
  EgaDisplayDevice, PioScanlineEngine and the `.pio` programs.
- `src/host/HostDisplayDevice.cpp` supplies the in-memory host backend.
- `examples/` and `apps/*_crt_diagnostics/` are firmware consumers.
- `tests/` is a standalone native CMake project. `picottl_tests` runs the
  existing harness; `picottl_host_tests` is its single CTest registration.
  `tests/support/` provides RecordingScanlineEngine and a recording SDK.
  Test-only copies compile actual engine/device implementations under renamed
  classes. They do not execute real PIO instructions or DMA transfers.

## Compatibility and scope

Follow [API evolution rules](docs/api_evolution.md). Preserve existing public
names, signatures, enumerator values and documented behaviour. Ask for explicit
approval before changing public API or established semantics; approval does
not silently amend the freeze policy. Keep existing examples compiling
unmodified. Do not add speculative abstractions or unrelated refactors.

Match the local style: four spaces, PascalCase types, camelCase functions,
k-prefixed constants and trailing underscores on private members. Preserve
C++17 and the existing no-heap/no-exceptions/no-RTTI framework policy. Do not
hand-edit generated fonts: use the generators in `tools/` and preserve
[provenance](THIRD_PARTY_LICENSES.md).

## Hardware-sensitive changes

PIO instructions, DMA configuration/chaining, GPIO mapping, clock/divider
selection, timing tables, framebuffer source replacement and engine start/stop
require a concrete technical rationale and focused regression evidence.
Inspect the current implementation, SDK and relevant errata before changing
these paths. Preserve ownership, rollback and restart guarantees.

The maintainer reports physical testing of all examples on Raspberry Pi
Pico 2 using Arm Cortex-M33, with an IBM 5151 and a Samtron SC-431E.
Preserve those results, the architecture and monitor models. Distinguish
maintainer-reported testing from checks you execute yourself. Do not infer electrical measurements, new configurations
or validation of later changes from that evidence. Compilation, host tests
and SDK recordings establish different evidence. Report any additional
hardware validation needed, without assigning its execution automatically
to the maintainer; the maintainer directs and accepts validation.

## Memory and concurrency

Preserve the [ownership and concurrency contracts](docs/architecture.md#5-object-ownership-and-lifetime).
Buffers and descriptors are borrowed; truthful capacities, alignment and
lifetime are caller obligations where the API cannot verify them. PicoTTL
objects are not generally thread-safe: lifecycle calls, rebinding and
retirement require serialization. Distinct packed pixels may share a byte
updated by read-modify-write. Live CPU drawing during SRAM DMA scanout may
tear; it does not authorize concurrent CPU writers. Stop output, retire the
binding and quiesce all users before freeing scanned storage. Do not add
locks, barriers or volatile qualifiers without a demonstrated requirement.

## Build organization

`cmake/PicoTTLPortableSources.cmake` is the canonical portable source list
shared by firmware `picottl` and host `picottl_tests`. Register new portable
implementations there, not in duplicate lists. Preserve the native host
build's independence from Pico SDK, ARM tools and firmware link scripts.
Recorder-specific hardware copies remain separate. Quality-tool allowlists
are deliberate analysis scope; review them when adding relevant sources.
The root CMake project is firmware-specific; do not invent a root host mode.

## Tests and quality

Run from the repository root, using separate out-of-source directories.
For native CMake, Ninja and a C++17 compiler:

```sh
cmake -S tests -B build-host -G Ninja
cmake --build build-host
ctest --test-dir build-host -N
ctest --test-dir build-host --output-on-failure --verbose --no-tests=error
```

For native GCC/Clang with working sanitizer runtimes (Linux is verified):

```sh
cmake -S tests -B build-sanitize -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DPICOTTL_ENABLE_SANITIZERS=ON
cmake --build build-sanitize
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure --verbose --no-tests=error
```

Infrastructure tests and the existing static-analysis wrappers:

```sh
python3 -m unittest discover -s tools/quality_tests -v
python3 tools/quality.py static clang-tidy --executable clang-tidy-18 \
  --build-dir build-host --output-dir reports/clang-tidy
python3 tools/quality.py static cppcheck \
  --build-dir build-host --output-dir reports/cppcheck
```

For the existing Arm firmware build, configure Pico SDK 2.3.0
(`PICO_SDK_PATH`) and the ARM toolchain as described in
[integration.md](docs/integration.md). Maintainer-reported physical tests
used Arm; do not extend those results to RISC-V:

```sh
cmake -S . -B build -G Ninja -DPICO_BOARD=pico2
cmake --build build
```

All relevant existing tests must keep passing. Add focused tests for changed
behaviour using the current harness. Never apply host sanitizers to firmware.
See [code_quality.md](docs/code_quality.md) for coverage, CodeQL, prerequisites
and report/baseline interpretation. Run available checks appropriate to the
change; disclose unavailable tools rather than claiming execution. Preserve
CI job names and permissions; do not silently weaken tests, sanitizers,
analysis baselines or API contracts.

## Workflow and reporting

Check branch and working-tree status first. Work on the maintainer-requested
branch; stop on a mismatch and do not switch branches or discard unrelated
changes. Follow the existing GitFlow/PR workflow based on `develop`, keeping
changes focused. Use English Conventional Commit messages as described in
CONTRIBUTING.md when proposing commits. Do not create commits, tags or push
unless explicitly authorized.

Report implemented changes, commands actually executed, observed results,
unexecuted checks, remaining risks and pending physical validation separately.
Do not turn planned checks or a passing build into unsupported guarantees.
