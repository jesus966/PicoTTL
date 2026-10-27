# AI Development Policy

## Development model

PicoTTL is a human-directed engineering project with predominantly AI-written
implementation and documentation. Nearly all source code and documentation
have been written using AI tools, including OpenAI Codex and ChatGPT. Those
tools perform implementation, code generation, test development, documentation
drafting, investigation and analytical assistance under maintainer direction.

The distinction is between authorship of implementation and responsibility
for engineering: AI writes most of the code and documentation; the human
maintainer directs the engineering and decides what is accepted.

## Human responsibility

The maintainer establishes goals and requirements, directs development,
evaluates alternatives, approves architectural decisions, reviews proposed
changes and defines investigation and validation requirements. The maintainer
accepts or rejects contributions and remains responsible for technical
decisions and releases. This does not assert individual review of every
generated line or imply that the maintainer manually authored most code.

The maintainer directs validation and accepts its evidence; this does not
require the maintainer to personally execute every hardware test or
measurement. Contributors or other testers may perform them.

The maintainer has physically tested all examples on a Raspberry Pi Pico 2
using the Arm Cortex-M33 configuration, an IBM 5151 and a Samtron SC-431E.
Preserve these
reported results and their scope. They establish observed operation in those configurations,
not an assertion that RISC-V execution has been validated, every electrical
parameter has been measured, or all future changes and monitors are validated.

## AI-assisted contributions

Human-written and AI-assisted contributions are welcome and face the same
technical requirements in [CONTRIBUTING.md](CONTRIBUTING.md). Contributors
remain responsible for correctness, security, testing, provenance and
compatibility with the project's [license](LICENSE), including checking for
improper reproduction of third-party code or data. AI generation is not a
guarantee of originality or licensing compatibility; see also
[third-party data provenance](THIRD_PARTY_LICENSES.md).

For substantial AI-generated code or documentation, include a brief PR
declaration describing the assistance and your verification. No particular
provider or assistant is required. Do not present generated work as manually
written. Per-line labels and mandatory AI commit trailers are not required.

## Verification and validation

Use the existing [quality workflow](docs/code_quality.md) and report evidence
at the level actually established:

| Evidence | What it establishes and its limits |
|---|---|
| Code and architectural review | Assessment of design, contracts and implementation; not a correctness guarantee. |
| Host functional tests | Observed behaviour of exercised production paths on a host. |
| SDK recorder tests | Recorded SDK calls and modelled state transitions; no physical PIO/DMA execution. |
| Sanitizers and static analysis | Diagnostics within their execution paths, models and configured scope; silence is not proof of safety. |
| Firmware compilation | Build/link compatibility for the selected board, SDK and toolchain; no real RP2350 execution. |
| Recorded execution on RP2350 | Behaviour observed on that board and configuration; not automatically electrical verification. |
| Physical measurements | Measured electrical levels, waveforms and timing under the recorded conditions. |
| CRT testing | Observed compatibility with the identified monitor, wiring, modes and clocks; not all CRTs. |

Never infer physical PIO/DMA timing, electrical correctness or CRT
compatibility from passing host tests, a recorder or firmware compilation.
No contributor or agent may claim physical validation without recorded
hardware evidence. Attribute maintainer-reported results as such; do not
present them as agent-executed tests. Report unexecuted checks and any
additional hardware work plainly.
The maintainer determines acceptance using that evidence; a merge does not
turn software-only verification into completed hardware validation.

## Transparency and limitations

AI tools can produce incorrect code, misleading explanations, incomplete
tests and unsupported claims. Human review can also miss defects. Evidence
and maintainer acceptance matter more than the identity of the author;
neither AI use nor automated checks provide an unconditional quality or
security guarantee. Keep the distinction between observed results,
assumptions and remaining risks visible.

## Policy scope

This policy applies to maintainers, contributors and AI-assisted development
workflows. It complements the existing contribution, API and architectural
rules rather than creating a separate approval process. Repository coding
agents should follow [AGENTS.md](AGENTS.md).
