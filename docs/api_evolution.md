# PicoTTL Public API Evolution Guidelines

This document defines what "frozen" means for the PicoTTL public API,
which changes are allowed, which are forbidden forever, and how new
functionality must be introduced. It complements
[architecture.md](architecture.md) (the layer reference) and
[design_principles.md](design_principles.md) (the project philosophy).

The contract in one sentence: **application code written against the
public API today must keep compiling, unchanged and with identical
behaviour, against every future version of PicoTTL.**

---

## 1. What "frozen" means

A frozen API is a promise about *existing* surface, not a prohibition on
growth:

- Every public name, signature, type, enumerator value and documented
  behaviour that exists today will keep existing, with the same meaning,
  forever.
- The API may **grow** at any time (new names, new members, new types,
  new backends), provided that growth is invisible to code that does not
  use it.
- Frozen refers to **source compatibility**, not binary compatibility.
  PicoTTL is a source library compiled into the application; ABI
  stability across versions is explicitly a non-goal.

The examples directory is the executable form of this promise: every
example must compile, unmodified, after every change to the framework.
An example that stops compiling is the definition of a broken freeze.

## 2. API tiers

Not every header carries the same promise:

| Tier | Headers | Promise |
|---|---|---|
| **Stable Public API** | `picottl/*.hpp` (PicoTTL.hpp, Display, DisplayDevice, DisplaySurface, Framebuffer, MonochromeFramebuffer, PackedFramebuffer, PixelFormat, Graphics, MonochromeBitmap, Color, VideoMode, Font incl. FontFamily, CodePage, Text, Terminal, AnsiTerminal), `picottl/fonts/*.hpp`, `picottl/cga/Colors.hpp`, `picottl/ega/Colors.hpp`, `picottl/host/HostDisplayDevice.hpp`, backend construction surface (`picottl/rp2350/{Mda,Cga,Ega}DisplayDevice.hpp`: Config + constructor + DisplayDevice interface) | Frozen. Additive evolution only. |
| **Backend SPI** | `picottl/video/*` (VideoTiming, ModeTimings) | For backend implementers. May change between minor versions; changes are documented but not frozen. |
| **Internal** | `picottl/rp2350/PioScanlineEngine.hpp`, everything in `src/` | No promise at all. May change or disappear at any time. Applications must never name these types, even though include chains make them technically visible. |

The stable build interface consists of the CMake target `picottl`, the
public include spellings under `picottl/`, and support for adding the
repository root with `add_subdirectory()` without creating executable
targets. Internal directory layout, CMake variables, and targets belonging
to examples, diagnostics, tests or tools are not part of the frozen API.

## 3. Additive evolution

A change is *additive* when a program that does not mention the new
name compiles and behaves exactly as before. Additive changes may ship
at any time without deprecation cycles.

### Examples of additive evolution

- ✔ New `VideoMode` enumerator values (existing values never change)
- ✔ New `Display` methods (`waitForVBlank()`, `text()`, ...)
- ✔ New `Graphics` primitives (`drawCircle()`, `fillRect()`, ...)
- ✔ New `Text` features (`clear()`, `scroll()`, `setColors()`,
  `setLineSpacing()`, attribute support)
- ✔ New `DisplaySurface` implementations (color framebuffers, double
  buffers, sub-surface views, host renderers)
- ✔ New `Font` resources (`fonts::kIbmCga8x8`, user fonts) and new
  `Font` fields **with default member initializers** (keeps every
  existing aggregate initialization valid)
- ✔ New backend `Config` fields **with default values**
- ✔ New RP2350 backends (`CgaDisplayDevice` and `EgaDisplayDevice`
  arrived this way)
- ✔ New host/desktop backends for testing and validation
- ✔ New compile-time configuration macros **with defaults preserving
  today's behaviour** (e.g. `PICOTTL_TEXT_PRINTF_BUFFER`)
- ✔ New overloads that cannot change overload resolution of existing
  calls
- ✔ Documentation improvements; internal refactoring below the frozen
  surface

### Examples of breaking changes (never allowed)

- ✘ Removing or renaming public methods, types, namespaces or headers
- ✘ Changing existing signatures (parameter types, return types,
  constness, noexcept)
- ✘ Changing or reusing existing enumerator values
- ✘ Reordering or removing fields of public structures (Config structs,
  `Font`, `Color`, `MonochromeBitmap`), or adding fields *without*
  default initializers
- ✘ Making previously public classes internal, or demoting a header to
  a lower tier
- ✘ Changing documented behaviour (clipping rules, cursor semantics,
  wrap behaviour, error-return conventions, cell-grid definition)
- ✘ Changing defaults in a way that alters the behaviour of existing
  code (Config defaults, color defaults, buffer sizes)
- ✘ Adding required parameters to existing constructors
- ✘ Introducing exceptions, hidden allocation or implicit hardware
  claiming anywhere reachable from the public API

### The grey zone (allowed with care)

- Adding members to *virtual* interfaces (`DisplayDevice`,
  `DisplaySurface`) is additive for applications but breaking for
  out-of-tree implementers. PicoTTL accepts this: these interfaces are
  implemented by backends, which belong to the framework's SPI side.
  When possible, new virtuals get a default implementation.
- Bug fixes that change behaviour are allowed only when the previous
  behaviour contradicted the documentation. If code in the wild may
  depend on the buggy behaviour, the fix is documented prominently.
- Compiler/SDK requirement bumps (C++ standard, Pico SDK version) are
  not API breaks but are treated as major events and documented.

## 4. Principles when adding new modules

1. **Additions enter the correct tier.** New application concepts go in
   `picottl/`; backend machinery goes in the SPI or internal tiers. When
   in doubt, start internal - promotion to public is additive, demotion
   is a break.
2. **New modules follow the layer rules**: dependencies point strictly
   downward, platform code only in backend directories, everything else
   portable C++17.
3. **One responsibility, documented purpose.** A new public class ships
   with a one-sentence purpose in its header, ownership/lifetime rules,
   and its stability tier marked at the top of the file.
4. **Resources are descriptors.** New resource types (fonts, bitmaps,
   palettes) follow the `Font`/`MonochromeBitmap` pattern: immutable,
   caller-owned, flash-friendly aggregates with defaulted fields.
5. **Config over constructors over setters.** Hardware wiring goes in a
   `Config` struct with safe defaults; required collaborators are
   constructor-injected (no two-phase initialization); optional behaviour
   arrives as additive setters.
6. **Every addition ships with a validation example** and, where
   geometry or data tables are involved, `static_assert`s binding any
   duplicated compile-time facts to their authoritative source.
7. **No speculative surface.** Nothing becomes public because it "might
   be useful"; it becomes public when an example or a real consumer
   needs it. Removing a mistake later is impossible - not adding it is
   free.

## 5. Deprecation policy

There is none, by design: nothing public is ever deprecated or removed.
The cost of this promise is paid at review time - every public addition
is reviewed as if it must be maintained forever, because it must. This
is why the public surface is kept deliberately small.

## 6. Checklist for any public change

- [ ] Is it purely additive? (If not: stop.)
- [ ] Do all examples compile unchanged?
- [ ] Is IBM MDA output bit-identical? (Run the hardware validation
      suite, Examples 01-17; Example 13 is the pixel-exact regression.)
- [ ] New fields/parameters have defaults preserving current behaviour?
- [ ] Correct tier, correct layer, purpose documented, example added?
- [ ] No exceptions, no hidden allocation, no implicit hardware use?
