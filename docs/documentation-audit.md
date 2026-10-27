# Documentation review

Review date: 2026-10-27. This is a documentation snapshot, not an additional
API specification or a claim that every implementation path is correct.

## Scope and method

Reviewed root Markdown, all Markdown under `docs/`, the example-43 guide,
issue/PR templates, public headers, numbered-example headers, application
build targets and the current CMake/workflow definitions. Cross-checks used
production implementations, the PIO instruction streams, and host/SDK-recorder
tests. Existing photographs and maintainer statements are attributed evidence;
this review performed no RP2350 execution, electrical measurement or CRT test.

The maintainer explicitly confirmed retaining the reported Pico 2 Arm tests
of all examples with the IBM 5151 and Samtron SC-431E. This is distinct from
measured voltage margins, RISC-V/RP2350B validation and later-change validation.
External electrical specifications and live GitHub rulesets were not independently
revalidated. Unverified claims are not promoted to measured facts.

## Documentation inventory

| Location | Role |
|---|---|
| [README](../README.md) | Overview, modes, build entry point, reference mapping and documentation navigation |
| [Getting started](getting-started.md) | New-user path, choosing a backend, one real example and host-only route |
| [Integration](integration.md) | Parent SDK project, submodule/vendor/FetchContent use and stable build target |
| [Architecture](architecture.md) | Layer and API explanations, ownership, memory, concurrency, GPIOs, diagnostics, code pages, connector and implementation appendix |
| [Graphics clipping](graphics_clipping.md) | Arithmetic, clipping, rasterization and pointer-resource limits |
| [DMA shutdown/restart](rp2350_dma_stop.md) | Channel topology, erratum prerequisites, restart and dedicated physical test procedure |
| [Code quality](code_quality.md) | Host instrumentation, report scope, baseline, CodeQL, permissions and rollout guidance |
| [Diagnostic triage](code_quality_findings.md) | Current interpretation of known and missing baseline diagnostics |
| [Design principles](design_principles.md), [API evolution](api_evolution.md) | Design constraints, source-compatibility tiers and additive evolution |
| [This review](documentation-audit.md) | Coverage assessment, fixes and outstanding gaps |
| [Contributing](../CONTRIBUTING.md) | Workflow, style, validation evidence, Conventional Commits and release preparation |
| [AI policy](../AI_POLICY.md), [agent instructions](../AGENTS.md) | Authorship, engineering responsibility and executable development instructions |
| [Changelog](../CHANGELOG.md) | Historical release record; historical test counts are not current suite counts |
| [Provenance](../THIRD_PARTY_LICENSES.md), [LICENSE](../LICENSE) | Code/data/image attribution and project license |
| [Security](../SECURITY.md), [conduct](../CODE_OF_CONDUCT.md) | Reporting and participation policies |
| [PR template](../.github/pull_request_template.md), [bug template](../.github/ISSUE_TEMPLATE/bug_report.md), [feature template](../.github/ISSUE_TEMPLATE/feature_request.md) | Submission prompts and evidence collection |
| [Example 43 guide](../examples/43_host_image/README.md) | USB image sender, wire protocol, timeout and session quarantine |
| `include/picottl/*.hpp` and `fonts/`, `cga/`, `ega/`, `host/`, `rp2350/`, `video/` subtrees | Normative public contracts plus explicitly identified backend SPI/internals |
| `examples/01..45`, `examples/96..98`, `apps/*_crt_diagnostics/` source headers | Per-feature setup, expected output, regression intent and application-specific controls |
| `docs/images/` | Four JPEG assets, not standalone measurement records |

No existing document was moved or renamed. The two new pages are the beginner
entry point and this review; existing reference paths remain available.

## Feature coverage assessment

Status describes the documentation as reviewed, followed by branch action.

| Feature / contract | Initial coverage and evidence | Result |
|---|---|---|
| Pico 2 Arm; RP2350A/B GPIO windows | Adequate in backend headers and architecture hardware policy; physical scope easy to overread | Retained constraints, attributed monitor evidence; board routing remains separate |
| Catalog modes and depth support | Adequate in VideoMode, backend supports()/supportsPixelFormat() and memory tables; overscan hard to find from overview | Beginner table lists all catalog modes and backend depths |
| Creation and build integration | Integration page exists, but its automatic backend example conflicts with stack guidance | Static local device and checked begin(); vendored tree includes canonical cmake module |
| SDK/native host prerequisites | Parent project owns SDK, but native pioasm/picotool setup was implicit | Requirements now distinguish host tools and cross compiler |
| Public API and configuration | Detailed headers; discovery weak for new users | API lookup and reader-specific links added, without another specification |
| begin/end, repeated start, mode changes | Adequate headers, device code and display lifecycle tests | Beginner lifecycle summary makes stop/rebind/validation timing visible |
| Failed configuration and resource ownership | Detailed engine headers/architecture; host configuration tests cover rollback/retry | Preserved invalidating configuration and retained-claim semantics |
| Buffer capacity, packing, 6-bit fields and alignment | Adequate PackedFramebuffer/engine contracts and capacity tests | Preserved byte-versus-word checks and caller truthfulness obligations |
| Framebuffer replacement and retirement | Adequate Display implementation and test_display_lifecycle | Linked at point of use; end() retains Graphics, accepted detach makes it inert |
| Graphics primitives and clipping | Adequate public header, graphics_clipping and exact-pixel tests | Linked reference retained; no unsupported fillRect/drawCircle API introduced |
| Text, fonts, code pages and formatting | Detailed Text/Font headers and code-page section; UTF-8 limitation buried | Lookup and beginner limitations expose byte rendering and bounded printf |
| Terminal cells, scroll, repaint, cursor/blink | Adequate Terminal header and tests; update() and geometry snapshot easily missed | Beginner rules highlight cell storage, periodic update and reconstruction |
| ANSI controls and SGR | Adequate header and parser tests, difficult to locate | Lookup explains 16-color palette, extended-color fallback and shared save slot; no complete VT guarantee |
| Custom timing | Inconsistent: architecture said any VideoTiming, while engine enforces representability and public begin takes VideoMode | Corrected distinction and linked existing precise constraints |
| Nominal/effective frequency | Adequate architecture formulas and VideoTiming/Display headers | Retained nominal-query meaning, divider coherence and measurement caveats |
| Timing PIO and DMA pipeline | Mostly detailed, but appendix said three segments per line | Corrected three/four rule from buildTimingBuffer; added data-flow diagram |
| DMA stop/restart | Technical detail useful, but organized as a historical fix and obsolete test snapshot | Functional title/sections; preserved E5 sequence, lifetime and physical procedure; updated recorder scope |
| Memory, multicore and flash/BOOTSEL | Detailed current ownership matrix and implementation-specific limitations | Linked rather than duplicated; no new synchronization promises |
| USB image transfer | Example 43 guide documents sizes, inactivity timeout, quarantine and firmware-reset recovery | Discoverability improved; protocol and sender left unchanged |
| Host tests and SDK recorder | Accurate CTest and instrumentation reference; old DMA/CodeQL prose understated current engine extraction | Corrected extraction-versus-published-report scope, kept physical limitations |
| Static-analysis triage | Incorrectly presented corrected timing arithmetic as outstanding; obsolete case totals | Current fixed status and stale baseline entry documented; no baseline modification |
| Firmware targets and diagnostics | Actual CMake has 45 examples, three demos and three apps; README used old diagnostics path | Corrected path and verified one concrete target/output |
| Compatibility, releases and licensing | API tiers and release workflow already documented; image section said none existed | Kept contracts/license; corrected image inventory while retaining attribution statement |
| Electrical interface and monitor compatibility | Pin mapping derivable; universal direct-wiring/voltage advice exceeded available measurement evidence | Qualified reference wiring; no substitute circuit or universal compatibility claim |

Primary implementation evidence: [Display](../include/picottl/Display.hpp),
[devices and engine](../src/rp2350/), [packing](../include/picottl/PackedFramebuffer.hpp),
[Text](../src/Text.cpp), [Terminal](../src/Terminal.cpp),
[ANSI parser](../src/AnsiTerminal.cpp), [host backend](../src/host/HostDisplayDevice.cpp),
[host build](../tests/CMakeLists.txt), [firmware build](../CMakeLists.txt) and
[workflows](../.github/workflows/). Tests are supporting evidence for exercised
software behaviour, not electrical specifications.

## Reader paths and hierarchy

| Reader | Path | Usability change |
|---|---|---|
| First-time display user | README -> Getting started -> matching example header -> electrical assessment | A short path avoids requiring the full architecture before building |
| Embedded integrator | Getting started or README -> Integration -> backend Config/public headers -> ownership | Host-tool and static-storage requirements are visible before copying code |
| Contributor | CONTRIBUTING -> API evolution/design principles -> architecture -> quality | Existing policy hierarchy retained; no extra approval framework |
| Timing/GPIO/DMA debugger | Architecture reading links -> implementation appendix -> DMA lifecycle -> diagnostics headers | Correct pipeline and scope distinctions precede low-level test procedure |

README remains an overview; the beginner guide is a practical tutorial and
conceptual entry point; headers remain API reference; architecture/DMA/clipping
remain detailed explanations. No documentation site or generated API system
is needed to achieve this navigation improvement.

## Confirmed corrections

- README's obsolete `diagnostics/` path now points to `apps/*_crt_diagnostics/`.
- Integration's multi-kilobyte automatic device became a static local after
  clock setup; begin() failure is checked. Its actual documented CMake and C++
  snippet was compiled externally, not copied into production examples.
- Vendoring instructions retain `cmake/`, required by the canonical source list.
- Architecture describes three or four timing entries per line, constrained
  custom timings and the absence of a public custom-timing begin overload.
- DMA documentation describes current shutdown/rearm and host engine coverage
  rather than historical fix identifiers, before/after narration or old totals.
- Diagnostic triage marks timing arithmetic corrected and its baseline entry
  missing. Published report filtering is distinguished from CodeQL extraction.
- Quality guidance lists all five check names without claiming access to live
  required-check settings or suggesting removal of already-required checks.
  Pull-request triggers are correctly described as unfiltered.
- Image inventory reflects the four existing assets. Electrical reference
  mapping is no longer presented as universal direct-connection approval.

## Open gaps and prioritized follow-up

1. **Hardware evidence:** collect a versioned test record with firmware commit,
   board/silicon revision, clock, monitor, wiring and observed results. Keep
   maintainer-reported example tests; separately record scope traces, voltage
   margins, input loading and dedicated DMA retirement tests when performed.
   This review does not infer measurements or erase reported monitor tests.
2. **Electrical reference:** substantiate monitor-specific interface assumptions
   with applicable specifications and measurements before presenting a definitive
   connection circuit. GPIO assignments alone are insufficient. Capture dates
   and revision metadata for photographs would improve traceability.
3. **Source-header comments:** ttl_timing.pio's opening comment still describes
   a combined active/front-porch three-segment line, and MonochromeBitmap.hpp
   describes Text consumption as future work although Font::glyph()/Text use
   it today. These are documentation-only follow-ups in files excluded from
   this branch's production/example modification scope; no runtime defect is
   asserted. Example 17 also prints Display geometry before begin(), while
   the facade header guarantees it after successful begin(); current devices
   expose default-mode metadata, but applications should not generalize that
   into a facade guarantee. The beginner guide points to the guaranteed
   order. Resolve these example/header descriptions in a separately scoped
   change rather than modifying examples here.
4. **Quality maintenance:** retire the obsolete VideoTiming baseline entry only
   through its reviewed process. Confirm live GitHub required checks separately;
   workflow names and documentation cannot establish server-side protections.
5. **USB evidence:** example 43 states approximate throughput. Treat it as an
   estimate unless a board/host/version measurement is recorded. Physical
   disconnect/reset behaviour and reset-time reconnection require their own tests.

No production defect was established by this documentation review. No new API,
wiring circuit, allocator, synchronization scheme or build mode is proposed.

## Validation record

Local links/anchors and fenced blocks are checked across repository Markdown.
Both Mermaid diagrams are parsed using Mermaid 11.4.1 in a temporary external
validation directory; no documentation dependency is added to the repository.
The diagrams describe source relationships, not measured timing.

The native host executable and CTest passed all 184 cases (one CTest entry);
30 quality infrastructure tests passed. Baseline-aware clang-tidy/cppcheck
completed with no new diagnostics. The documented Example 17 target produced
its UF2. The integration CMake/main.cpp snippet also built externally, with
only the checkout path substituted and installed SDK host-tool package paths
supplied. An initial consumer build selected an unusable Watcom cl.exe for
pioasm; configured SDK packages resolved that environmental prerequisite,
which is now documented.

No full 51-target rebuild, sanitizer rerun, coverage capture, CodeQL rerun,
physical execution or electrical/CRT measurement was performed in this review.
Changes are Markdown only; tests, examples, CMake, PIO, workflows, production
headers/sources and LICENSE are unchanged. Future revisions should refresh
this validation record rather than treating snapshot counts as guarantees.
