# PicoTTL Design Principles

The long-term philosophy of the project. Every contribution - including
future ones by the original authors - is measured against these
principles. When a change conflicts with one of them, the change is wrong
or the principle must be amended explicitly, in this document, first.

## API and compatibility

1. **Public API stability over implementation convenience.** Application
   code written today must keep compiling as backends are added. The
   public surface evolves additively; existing signatures never change.
2. **Hardware details never leak into application code.** PIO, DMA,
   scanlines, sync pulses, porches and timing tables are backend
   vocabulary. Applications speak in displays, surfaces, graphics, text
   and video modes. The only sanctioned exception is the backend `Config`
   struct, where the application assigns physical resources.
3. **Backends are replaceable.** The measure of the architecture is that
   switching MDA -> CGA -> host-renderer changes one construction line.
4. **Internal implementation may evolve freely** while preserving the
   public API. Nothing in `picottl/video/*` or platform internals is a
   compatibility promise.

## Design

5. **Every module has one responsibility.** A class that only forwards
   calls, or that accumulates unrelated duties, is a design error.
6. **Prefer composition over inheritance.** Inheritance is reserved for
   genuine interface/implementation relationships (`DisplayDevice` and
   its backends).
7. **Avoid overengineering. Add abstractions only when they solve an
   actual problem.** No speculative interfaces, no reserved fields, no
   embedded command formats or mini instruction sets "for later".
   Extensibility comes from interfaces and object-oriented design, not
   from anticipating every future requirement in today's data layouts.
8. **Prefer explicit C++ abstractions over encoded data structures.**
   When data must be packed for hardware, the packing lives in one
   documented place inside the backend.
9. **Prefer readability over cleverness.** The framework must be
   understandable by another developer years from now.

## Behaviour

10. **Deterministic behaviour is preferred over maximum performance.**
    Video generation must be jitter-free by construction (hardware-paced,
    not CPU-paced); everything else is allowed to be merely fast enough.
11. **Never allocate memory unless clearly documented.** No hidden heap
    use, ever. Large buffers are caller-provided; small fixed-size
    internals are embedded, documented and visible in the map file.
12. **Never claim hardware resources implicitly.** The application
    decides every PIO block, state machine, DMA channel and GPIO. The
    framework validates, configures, and releases them deterministically.
13. **Fail explicitly, never violently.** No exceptions, no `panic()`, no
    hidden resets, no terminating the application. Fallible operations
    return explicit results and leave the system in a safe state.
    `assert()` is for debug-build contract checking only.

## Process

14. **Keep the framework testable on desktop systems.** Only platform
    backends may depend on the Pico SDK; everything above `DisplayDevice`
    must compile as portable C++17 so it can be unit-tested, inspected
    and debugged on a host.
15. **Optimize only after measuring.** Accept simple designs (e.g.
    per-pixel virtual dispatch at drawing time) until profiling proves a
    problem; then optimize additively.
16. **Every public class must have a clearly documented purpose.** If the
    one-sentence purpose is hard to write, the class is wrong.
17. **Every feature ships with a minimal example.** Examples are the
    executable specification and the regression suite for the public API.
18. **Each step must compile.** The main branch is never broken; large
    features are built as sequences of small, complete steps.

## Scope

19. **Standards are defaults, not limits.** IBM-exact behaviour is the
    curated default of the mode catalog, never the boundary of the
    framework. Non-standard, experimental and diagnostic video modes are
    first-class citizens, added through the same additive mechanisms as
    the historical ones. Nothing in the architecture may assume that
    "MDA" and "720x350 @ 50 Hz" are synonyms: standards are families,
    modes are members, and the raster engine is timing-agnostic.

## Color and signal model

20. **Pixel values are abstract indices; only the backend assigns their
    meaning.** `Color` is an opaque value everywhere above the backend:
    Graphics and Text only read and write it, framebuffers store its low
    bits verbatim (natural depth truncation is the only transformation),
    and no shared layer may ever assume RGB, palettes, brightness,
    channel ordering or color depth. `Color{4}` is red on CGA and
    undefined elsewhere; the same index may mean anything on another
    backend. The only cross-backend conventions are: index 0 renders
    dark, and bit 0 lights a monochrome pixel. Named color constants are
    OPTIONAL, BACKEND-SCOPED conveniences (e.g. `picottl/cga/Colors.hpp`)
    - they document one backend's verbatim mapping and are never part of
    the rendering model; applications may always use raw indices.
    Generic color names in the common API are forbidden, because a
    universal name would lie on some backend.

21. **Backend configurations speak the monitor's language.** Each
    backend's `Config` names its standard's electrical signals
    explicitly (`hsyncPin`, `vsyncPin`, `redPin`, `bluePin`, ...), as if
    it were the connector's datasheet. Configs are deliberately
    per-backend and non-polymorphic - the portability boundary is
    `DisplayDevice`, so there is nothing a shared signal-descriptor
    type would abstract: EGA will name its six color signals, other
    standards will name whatever their connectors carry (a deliberate
    application of principles 7 and 8 - a generic descriptor table
    would be an encoded data structure solving no real problem, and
    would trade compile-time-checked field names for a forever-growing
    role vocabulary). Platform limitations (e.g. the RP2350's
    consecutive-pin PIO mapping) never shape these names: they are
    validated privately at `begin()`, which returns false for
    assignments the platform cannot realize.
