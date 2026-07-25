# Contributing to PicoTTL

Thank you for your interest. PicoTTL is a deliberately disciplined
project: the rules below are not bureaucracy, they are the mechanism
that keeps a frozen public API and three hardware-validated backends
trustworthy. Please read them before opening a pull request.

## The three governing documents

1. [docs/design_principles.md](docs/design_principles.md) - the
   project philosophy. Every change is measured against it.
2. [docs/api_evolution.md](docs/api_evolution.md) - what "frozen"
   means and which changes are additive.
3. [docs/architecture.md](docs/architecture.md) - the layer reference.

If a change conflicts with a principle, either the change is wrong or
the principle must be amended explicitly - in the document, first.

## Architecture-first workflow

Design is reviewed **before** code is written.

- For anything beyond a trivial fix, open an issue describing the
  problem, the proposed design, its alternatives, and how it interacts
  with the frozen API. Expect the design to be discussed and possibly
  reshaped; that is the normal path (most of PicoTTL's features went
  through multiple design rounds).
- New abstractions need a real consumer. Speculative interfaces,
  reserved fields and "for later" hooks are rejected on principle.

## Additive evolution and backward compatibility

The public API contract: **application code written against the public
API today must keep compiling, unchanged and with identical behaviour,
against every future version.**

- Existing public names, signatures, enumerator values and documented
  behaviour never change. Growth is additive (new names, defaulted
  struct fields, new enumerators, new backends).
- The examples directory is the executable form of this promise: every
  example must compile **unmodified** after your change. An example
  that stops compiling defines a broken freeze.
- Backend SPI (`picottl/video/*`) and internals (`src/`, the scanline
  engine) may change freely - see the tier table in
  [docs/api_evolution.md](docs/api_evolution.md).

## Documentation-first policy

Documentation ships **with** the change, in the same pull request:

- Public headers carry the normative API documentation. New surface is
  documented at the same depth as the existing one.
- Architecture-level changes update
  [docs/architecture.md](docs/architecture.md).
- Every new example header documents: what it **validates**, the
  **expected output** on a real monitor, the **wiring**, and which
  **regressions** it would reveal.

## Hardware validation methodology

Anything that touches the signal path (timings, PIO programs, DMA,
pixel formats, pins, clocks) must be validated on a real monitor
before it is considered done.

- State in the PR which monitor (make/model), which wiring (the
  reference connector unless justified), which system clock, and what
  you observed. Photos of the CRT are welcome.
- Use patterns with **horizontal features** when validating sync:
  solid fields and full-height bars cannot reveal capture failures
  (this cost us a two-day diagnosis once).
- The diagnostics applications (`apps/*_crt_diagnostics/`)
  and the numbered examples are the regression surface. If your change
  could alter any validated visual output, re-run the relevant ones.
- No hardware? Contributions to the platform-independent layers can be
  validated with the host suite alone - say so in the PR, and a
  maintainer will hardware-test before merge.

## Automated tests

```sh
cmake -S tests -B build-host -G Ninja
cmake --build build-host
./build-host/picottl_tests
```

- The suite must pass (currently 56 tests, 0 failures).
- Platform-independent behaviour changes need new tests in `tests/`.
- The Pico build must also compile cleanly:
  `cmake -S . -B build -G Ninja && cmake --build build`.

## Coding style

Match the surrounding code; in particular:

- C++17, Pico SDK only - no external libraries, no Arduino.
- No heap allocations, no exceptions, no RTTI; errors are `bool`
  returns; `assert()` is a debug-only contract check.
- The framework never claims hardware not listed in a backend
  `Config`, and never takes interrupts.
- Naming: `PascalCase` types, `camelCase` functions/variables,
  `kPascalCase` constants, trailing underscore for private members.
- 4-space indent, ~76-column comment wrap, `///` Doxygen-style docs on
  public surface, SPDX license identifier in every file header.
- Comments explain **why**, and document contracts; they do not
  narrate code.

## Pull request expectations

- One concern per PR. Small is beautiful.
- Include: the design rationale (or a link to the design issue), test
  results (host suite + Pico build), hardware validation notes where
  applicable, and documentation updates.
- Generated files (`src/fonts/*.cpp`) are never edited by hand -
  change the generator in `tools/` and regenerate.
- Commit style follows the existing history: `feat:`, `fix:`,
  `refactor:`, `docs:` prefixes.

## Licensing

By contributing you agree that your contribution is licensed under the
project's MIT license.
