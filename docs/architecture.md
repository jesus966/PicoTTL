# PicoTTL Architecture

PicoTTL is a reusable C++17 framework for generating classic TTL video
standards on the Raspberry Pi Pico 2 (RP2350). **It is not an IBM MDA
library** - IBM MDA was simply the first implemented backend, followed
by IBM CGA and IBM EGA. The architecture is designed so that further
TTL standards can be added as backends without redesign and without
breaking application code.

This document is the frozen architectural reference. It distinguishes
throughout between **architectural concepts** (binding for every backend,
present and future) and the **current RP2350 implementation** (free to
change at any time).

See also: [design_principles.md](design_principles.md).

---

## 1. The application model

Application developers think exclusively in terms of:

- **Display** - the display they are driving
- **VideoMode** - what they want to show (e.g. `VideoMode::IbmMda`)
- **DisplaySurface** - where pixels go
- **Graphics / Text / Terminal** - how they draw

They never think about PIO, DMA, GPIO configuration details, scanlines,
raster generation, sync pulses or timing tables. Those concepts do not
appear in application code, application headers or this section of the
documentation - they exist only inside backends.

```cpp
#include "picottl/PicoTTL.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

picottl::rp2350::MdaDisplayDevice::Config config;
config.hsyncPin = 2;                                // wiring decision

picottl::rp2350::MdaDisplayDevice device(config);   // backend choice
picottl::Display display(device);                   // the abstraction
display.begin(picottl::VideoMode::IbmMda);
```

The single deliberate exception: a backend `Config` struct names physical
hardware resources (PIO block, state machine, DMA channels, GPIO pins),
because *the application always decides which resources the framework may
use* (see §7). This is wiring vocabulary, not implementation leakage.

## 2. Layers

Dependencies point strictly downward; each layer knows only the layer
directly below it.

```
Application
    ↓
Future layers               (console, widgets, panels, shell - built ON
    ↓                        TOP of Terminal/AnsiTerminal, additively)
AnsiTerminal                (incremental ANSI/VT parser over Terminal)
    ↓
Terminal                    (terminal state machine; cell buffer = source of truth)
    ↓
Text                        (pure character renderer)
    ↓
Graphics                    (drawing primitives + raster operations)
    ↓
DisplaySurface              (abstract pixel destination)
    △ implemented by Framebuffer / PackedFramebuffer<N> (pixel memory)
    ↓
DisplayDevice               (backend interface; Display is the app facade over it)
    ↓
Backend internals           (per standard, per platform - free-form)
```

The Text and Terminal APIs are FROZEN (hardware-validated on a real
IBM 5151). New terminal-adjacent functionality is never added to them:
it arrives as additive layers above Terminal.

Two invariants of the text subsystem:

- **The framebuffer is a cache.** The authoritative state is always the
  logical terminal cell buffer; any pixel corruption, backend reset or
  framebuffer replacement is recoverable exclusively through
  `Terminal::repaint()`.
- **Time is injected.** `Terminal::update(nowMs)` is driven by an
  external monotonic clock supplied by the application, and its state is
  a pure function of the timestamp (never of the call rate). No timers,
  interrupts or SDK dependencies exist inside Terminal - host, RP2350
  and every future backend drive the same function.

One invariant of the whole stack (design principle 20):

- **Pixel values are abstract indices; only the backend assigns their
  meaning.** Every layer from Application down to Framebuffer treats
  `Color` as an opaque value: Graphics and Text pass it through,
  packed framebuffers store its low bits verbatim, and the electrical
  (or other) interpretation happens exclusively where the pixels leave
  the system - in the display backend. This is what lets the same
  drawing and text code run on MDA's VIDEO/INTENSITY pair, CGA's RGBI
  wires, and every future signal set without modification.

Everything from the application layer down to `DisplayDevice` is
portable C++17 - the same Graphics, Text and Terminal code runs
unchanged on every backend. Backend internals are the only place
platform SDKs may appear.

## 3. Stable public API vs internal backend SPI

The public surface is divided into tiers with different promises. The
**normative, always-current tier table lives in
[api_evolution.md](api_evolution.md), section 2** - it is deliberately
not duplicated here (the two copies once diverged). In summary:

- **Stable Public API** - everything under `picottl/*.hpp`, the fonts,
  the backend-scoped color tables, the host device and each backend's
  construction surface (`Config` + constructor + the `DisplayDevice`
  interface). Frozen: existing signatures never change; evolution is
  strictly additive.
- **Internal Backend SPI** - `picottl/video/*` and the scanline
  engine. For backend implementers; may change between minor versions.

Notes:

- `DisplayDevice` is public because applications hold one and pass it to
  `Display`, but applications never *call* it. Implementing it is a
  backend concern.
- `PioScanlineEngine.hpp` is included by the public backend header out of
  technical necessity (by-value composition, no heap). Inclusion does not
  make it public: applications must never name that class.
- `VideoTiming`, porch widths, blanking, sync polarity and timing
  compilation are backend implementation details. Applications know only
  `VideoMode`.

## 4. Core abstractions

### Display (frozen)

The central application-facing class. Construction injects a
`DisplayDevice`; `begin(VideoMode)` starts output; `width()`, `height()`,
`refreshRateMilliHz()` describe the active mode; `graphics()` and
`surface()` expose the drawing layers. Further members (a `text()`
accessor, vertical-blank waiting) would be added here additively.

### VideoMode (frozen)

A plain enumeration of the **curated video modes**. Standards and modes
are deliberately separate concepts: a *standard* (MDA, CGA, EGA) is a
family implemented by one backend; a *mode* is one concrete
raster configuration. One backend may serve several modes -
`MdaOverscan` joined `IbmMda` this way, and further variants (e.g. an
`MdaFullRaster`) arrive as additive enumerators handled by the same
MDA backend.

IBM-exact timings are curated defaults, never an architectural limit:
the raster engine underneath is timing-agnostic and accepts any
`VideoTiming`, so non-standard, experimental and diagnostic modes are
first-class. A backend advertises its supported set through
`DisplayDevice::supports()`; `begin()` fails explicitly for the rest.
New enumerators are added over time; existing values never change.

There is deliberately no `VideoStandard` enum: family membership is
expressed by each backend's internal mode->timing lookup (a null result
means "not my family"). A separate standard table would be a second
source of truth with no consumer; a `standardOf()` helper can be added
to the SPI later if one ever appears.

### DisplayDevice (frozen interface, additive evolution)

One implementation per video standard per platform. Owns all
standard-specific knowledge. Interface: `supports()`, `begin(VideoMode)`,
`end()`, `isActive()`, `width()`, `height()`, `refreshRateMilliHz()`.

### DisplaySurface (frozen)

A rectangular pixel destination, decoupling *rendering* from *storage*:

```cpp
class DisplaySurface {
public:
    virtual std::uint16_t width() const = 0;
    virtual std::uint16_t height() const = 0;
    virtual void setPixel(std::uint16_t x, std::uint16_t y, Color color) = 0;
    virtual Color pixel(std::uint16_t x, std::uint16_t y) const = 0;
    virtual void fill(Color color) = 0;
};
```

Graphics and Text draw into a `DisplaySurface`, never into a framebuffer
directly. The shipped implementations wrap caller-provided packed
framebuffers (`PackedFramebuffer<N>`, 1/2/4/6/8 bpp); future
implementations may be double buffers, host-side renderers or virtual
surfaces - with no change to Graphics, Text or applications.

Naming rationale: *Canvas* implies the object owns drawing operations
(those belong to Graphics); *RenderTarget* is GPU jargon foreign to this
domain; *Framebuffer* names a storage technique, not a role.
*DisplaySurface* states exactly what it is.

Performance note: per-pixel virtual dispatch is accepted deliberately -
rendering happens at drawing time, never on the signal path. If profiling
ever shows a bottleneck, batched access (row spans) can be added to the
interface additively.

### Graphics / Text / Font

`Graphics` provides drawing primitives over a `DisplaySurface` (pixels,
lines, bitmaps, clear; signed coordinates with clipping). `Text`
provides character output on a fixed cell grid (80x25 with the MDA
9x14 font): cursor addressing in cells, `putChar`/`print`/`printf`.
Both are platform-independent and depend only on the layer below.

`Font` is an immutable resource descriptor over flash-resident glyph
data; fonts are owned by nobody and referenced by `Text`. There is
deliberately no GlyphRenderer class: glyph rendering is the composition
of `Font::glyph()` and the opaque `Graphics::drawBitmap()` - a separate
class would only forward calls. Text attributes (colors, underline)
arrived without needing one, confirming the decision; if a future
feature ever complicates this, a renderer is extracted inside `Text`
without public API change.

## 5. Object ownership and lifetime

The framework performs **no hidden allocations** and owns **no objects it
did not create internally as plain members**. Ownership is always with the
application:

| Object | Owner | Lifetime rule |
|---|---|---|
| `Display` | Application | Must not outlive its `DisplayDevice`. |
| Backend device | Application | Must outlive the `Display` (and any surface it exposes). Its destructor stops output and releases claimed hardware. |
| `Config` structs | Application | Copied during construction; may be destroyed immediately afterwards. |
| Framebuffer memory | Application | Caller-provided; must outlive the surface that wraps it. |
| `DisplaySurface` | Application (or embedded in a backend as a plain member) | Must outlive any `Graphics`/`Text` bound to it. |
| Fixed-size backend internals (e.g. the MDA signal table) | Embedded by value in the backend object | Live and die with the backend; size is compile-time constant and visible in the map file. |

Safe destruction order is therefore the reverse of construction:
`Text`/`Graphics` -> `DisplaySurface` -> `Display` -> backend device ->
framebuffer memory.

## 6. Error handling policy (project-wide)

- **Never throw exceptions.** The framework is compatible with
  `-fno-exceptions` builds.
- **Never `panic()`**, never reset hardware behind the application's
  back, never terminate the application.
- **Explicit results**: operations that can fail return `false` (or, if a
  future need arises, a richer status type added additively). Failure
  leaves the system in a safe, inactive state.
- Backends **check availability before claiming** resources (e.g. testing
  whether a DMA channel is already claimed) precisely to avoid SDK-level
  panics.
- `assert()` is reserved for programming errors (contract violations) and
  only in debug builds; release behaviour never depends on asserts.

## 7. Memory policy

- **No hidden heap allocations** - the framework never calls
  `new`/`malloc`. No exceptions to this rule.
- **Deterministic RAM usage** - all memory is either caller-provided or a
  fixed-size member of an object the application instantiates.
- **Caller ownership whenever practical** - large buffers (framebuffers)
  are always provided by the application.
- **Fixed-size internal buffers are acceptable** when they materially
  simplify the API (e.g. the ~4.4 KB MDA signal table embedded in
  `MdaDisplayDevice`), provided they are documented and visible in the
  map file.

## 8. Hardware resource policy

The framework **never silently claims hardware**. The application always
decides, via the backend `Config`:

- which PIO block and state machine
- which DMA channels
- which GPIO pins

The framework validates the choices, claims exactly those resources,
fails gracefully (returns `false`) if any is unavailable, and releases
them in the backend destructor. Global machine state (system clock,
voltage) is never touched: clock policy belongs to the application.

## 9. Platform independence

Only `src/rp2350/` and `include/picottl/rp2350/` may include Pico SDK
headers. Everything else is portable C++17.

This enables the **host (desktop) backend**
(`host::HostDisplayDevice`) - a `DisplayDevice` that renders into
memory instead of generating signals - used by the automated test
suite for unit tests, rendering validation, framebuffer inspection and
regression tests on Windows/Linux/macOS. Because the portability
boundary is `DisplayDevice`, application and framework code above it
runs on the host unchanged; the architecture requires only that no
code above `DisplayDevice` ever gains an SDK dependency.

## 10. Adding a future backend (worked examples)

### Adding a new mode to an existing standard (e.g. MDA Overscan)

1. Add the `VideoMode` enumerator and its entries in the public geometry
   tables (`modeWidth`/`modeHeight`) - additive.
2. Add its `VideoTiming` entry to the SPI catalog; `static_assert`s tie
   the public and SPI tables together.
3. Add one row to the backend's internal mode->timing lookup (and grow
   its embedded timing storage if the new raster is larger).

`Display`, `Graphics`, `Text`, `DisplaySurface` and existing application
code remain unchanged - `display.begin(VideoMode::MdaOverscan);` is the
only new line an application writes.

### IBM CGA (implemented, hardware-validated)

The recipe above, executed: `VideoMode::IbmCga640`/`IbmCga320`, two
timing entries, `rp2350::CgaDisplayDevice` reusing the internal raster
engine with a 4-bit RGBI pixel program, `PackedFramebuffer4`. The
backend Config names the monitor's signals (`redPin`, `bluePin`,
`intensityPin`, ...); pixel value = IBM color number, verbatim.

Application change required: **one line** (the device construction) -
proven by diffing the ANSI serial terminal examples (24 vs 33).

### IBM EGA (implemented, hardware-validated)

Same recipe: `VideoMode::IbmEga350`, one timing entry,
`rp2350::EgaDisplayDevice` with IBM's Primary/Secondary signal naming
and the rgbRGB bit order (pixel value = IBM EGA 6-bit color value,
verbatim). Because IBM EGA is hardware-COMPATIBLE with CGA timings,
the EGA device also serves `IbmCga640`/`IbmCga320` by referencing the
existing catalog entries (compatibility, not duplication); the two
raster families use different pixel crystals, so switching between
them is the application's reclocking decision.

One RP2350 implementation limitation surfaced here (and is documented
in the backend header): the PIO shift register refills in 32-bit
words, so the scanout stream depth must divide 32. Six-wire EGA is
therefore scanned as `Packed8` (one byte per pixel, low 6 bits driven);
`Packed6` remains a valid portable memory format that the host backend
handles. Wire count and stream depth are independent engine
parameters.
layer (internal helpers), invisible above `DisplayDevice`.

### Hercules (a counter-example: a mode, not a backend)

Hercules is instructive precisely because it is NOT a new backend.
The 1982 Hercules card existed to add pixel-addressable graphics
(720×348) to the MDA monitor - same 5151, same signal set, same
18.43 kHz raster. PicoTTL's MDA backend is already pixel-addressable
at 720×350, a strict superset of that capability (the Hercules
four-bank memory interleave is a 6845 controller artifact with no
counterpart in PicoTTL's rendering model). If byte-exact Hercules
raster timing (348 active lines) were ever wanted for authenticity,
the standards-vs-modes taxonomy gives the answer: an additive
`VideoMode` served by the existing MDA backend, exactly like
`MdaOverscan` - one enumerator, one timing entry, no new backend.

---

## 11. The CRT diagnostics applications

PicoTTL ships one **official diagnostics application per backend
family** (not examples): the project's permanent hardware and framework
validation suites. Every present and future backend is brought up with
its diagnostics application before higher-level APIs are exercised, and
the on-screen output is part of the regression surface - a backend
change that alters it is a regression by definition.

- `apps/mda_crt_diagnostics/` - `picottl_mda_crt_diagnostics`, the
  MDA-family tool (IBM 5151 and compatibles).
- `apps/cga_crt_diagnostics/` - `picottl_cga_crt_diagnostics`, the
  CGA-family tool (IBM 5153 and compatible RGBI monitors); adds the
  color section (solid colors, RGBI staircases, bit planes, primaries,
  intensity ramp, transitions, ANSI colors, terminal behavior).
- `apps/ega_crt_diagnostics/` - `picottl_ega_crt_diagnostics`, the
  EGA-family tool (IBM 5154 and compatibles); adds the 64-color grid,
  rgbRGB bit planes, per-gun level ramps, gray ramp, default-palette
  verification and cross-crystal mode switching (its Mode select
  pattern demonstrates application-side reclocking).
- `apps/diagnostics_common/` - the shared library underneath: the
  backend-agnostic pattern framework (`DiagnosticPattern.hpp`), the
  generic pattern catalog (`Patterns.hpp`) and the BOOTSEL helper.

The applications deliberately remain separate: different monitor
families, different diagnostic goals, independent evolution. They share
*library code*, never each other's application code. Backend-specific
patterns (e.g. the CGA color set) live in their application and may
rely on that backend's *documented public color contract* - they still
derive all geometry from the context.

The shared rules mirror the framework's:

- patterns query all geometry from the Display API and all colors from
  a pixel-format-derived palette - never concrete mode numbers (the
  overscan geometry is constraint-derived and may change after hardware
  validation with zero diagnostics changes);
- behaviour branches on queried capabilities, never on backend
  identity (future capability queries arrive as additive
  `DisplayDevice` methods);
- mode switching uses exactly the public application API
  (`end()` / `setFramebuffer()` / `begin()`).

## 12. Backend memory reference

Documentation convention for every current and future backend: the
expected RAM footprint of each mode, so application authors can budget
before writing code. Framebuffers are caller-provided; timing tables
are embedded in the device object (sized for the largest supported
mode). The RP2350 has 520 KB of SRAM.

| Backend | Video mode | Typical format | Framebuffer | Timing table |
|---|---|---|---|---|
| MDA | IbmMda 720x350 | Mono1 / Packed2 | 31,500 / 63,000 B | 5,920 B |
| MDA | MdaOverscan 736x352 | Mono1 / Packed2 | 32,384 / 64,768 B | (shared) |
| CGA | IbmCga640 640x200 | Packed4 | 64,000 B | 4,192 B |
| CGA | IbmCga320 320x200 | Packed4 | 32,000 B | (shared) |
| EGA | IbmEga350 640x350 | Packed8 | 224,000 B | 5,824 B |
| EGA | IbmCga640 (compat) | Packed8 | 128,000 B | (shared) |
| EGA | IbmCga320 (compat) | Packed8 | 64,000 B | (shared) |

Guidance: a full EGA application (Packed8 framebuffer + backend +
80x25 terminal cells + SDK/stack) uses roughly 250 KB, leaving ~270 KB
free. Double-buffering the EGA framebuffer fits but consumes most of
the remaining headroom; memory-constrained applications can use
narrower formats at the documented wire cost. Fonts live in flash
(2,048-7,168 B each), linked only when referenced.

## 13. The showcase demonstrations

`examples/96_mda_demo`, `examples/97_cga_demo` and `examples/98_ega_demo`
are the **official PicoTTL showcase demonstrations**: polished,
historically authentic programs that present the complete capabilities
of each supported IBM display standard - in the spirit of the
demonstration disks that shipped with period hardware.

They are deliberately **three independent applications**, not one demo
with backend switching: an MDA monitor cannot display CGA or EGA, and a
CGA monitor cannot display EGA. Each demonstration is a self-contained
program dedicated to the one monitor its audience owns, configured
exactly as recommended for that hardware. They share utility code
(`examples/demo_common`: filled shapes, aspect-corrected ellipses,
ordered Bayer dithering, era-authentic wipe/fizzle transitions, CP437
boxes, typewriter output, banner lettering from the machine's own
character generator, and the shared technical-information and closing
screens) but never each other's application code - the same sharing
policy as the diagnostics applications.

Each demonstration follows the same arc: an animated introduction, a
sequence of scenes exercising that standard's defining capabilities
(text attributes / windows / terminal on MDA; the RGBI gamut, palettes,
dithering and sprites on CGA; the 64-color gamut, gun-level gradients,
per-pixel shading and smooth scrolling on EGA), a diagnostics-style
technical information screen whose every value comes from the public
API, and a closing screen that remains indefinitely.

They serve four purposes: demonstrate the capabilities of each IBM
video standard; provide production-quality reference implementations
for structuring complete PicoTTL applications; act as visual regression
tests for future PicoTTL releases (their output is part of the
regression surface, like the diagnostics); and celebrate the original
IBM display hardware. Historical authenticity outranks visual excess:
nothing on screen would look out of place on an IBM PC of the era.

## 14. Code pages

### The byte-to-glyph model

`Text` renders **bytes**, not Unicode. The pipeline is:

```
byte (0..255)  ->  glyph index  ->  Font  ->  glyph bitmap
```

For the single-byte IBM code pages the byte-to-index step is the
**identity**, because PicoTTL follows the original IBM model: every
font dataset is a complete, independent 256-glyph set stored in its
code page's native byte order. This is exactly how the hardware
worked - DOS's `MODE CON CP SELECT` loaded a whole font from `EGA.CPI`
into the character generator; there was never a base font with
supplemental glyphs. A byte whose index falls outside the active font
renders as the fallback glyph: a blank opaque cell (cell-grid
coherence is preserved). The renderer never knows *which* code page it
is drawing - it only consults the active font.

A future multi-byte pipeline (UTF-8 -> Unicode codepoint -> per-font
index mapping -> glyph) slots into the same model as additive layers;
nothing in today's contract constrains it.

### CodePage, Font metadata and FontFamily

- `CodePage` (`picottl/CodePage.hpp`) is an enum identifying the
  encoding a dataset renders: `Cp437` (the default) and `Cp850`.
  Future pages (CP852, CP858, ...) are additive enumerators.
- `Font` is self-describing: alongside the glyph data it carries its
  metrics, glyph count, underline row and `codePage` metadata (a
  defaulted field - every pre-existing aggregate initialization stays
  valid). `Font::glyph()` takes `std::uint16_t` (widened from
  `unsigned char`, source-compatible) so larger future fonts index
  naturally.
- `FontFamily` is a plain collection of datasets sharing **identical
  metrics** - the invariant that allows runtime switching without
  recreating `Text` or `Terminal` (every geometry snapshot stays
  valid). Lookup keys off each member's own `codePage` metadata, so
  adding a page to a family never changes the struct. Three families
  ship: `kIbmMda9x14Family`, `kIbmCga8x8Family`, `kIbmEga8x14Family`.

Selection API: construct `Text` with a family and call
`Text::setCodePage()` / `Terminal::setCodePage()`; the `Terminal`
variant repaints on success, so the whole screen reinterprets
instantly from the byte-valued cell buffer - authentic
`MODE CON CP SELECT` semantics. `Text::setFont()` remains the
low-level escape hatch for custom fonts (it detaches the family;
`setFontFamily()` restores managed mode). Backend-driven automatic
font selection was deliberately **rejected**: `Text` and `Terminal` do
not know a backend exists (layer purity), the app - not the backend -
owns the font choice, and fonts stay opt-in flash resources. A family
header is itself an explicit opt-in that knowingly links all member
datasets.

### CP437 vs CP850: history and when to use which

**Code page 437** is the character set burned into the original IBM
PC's ROM (1981): US ASCII plus box-drawing characters, some lowercase
Western-European accents, Greek letters and mathematical symbols. It
is the *only* historically correct choice for original MDA/CGA/EGA
hardware and US-market DOS software, and it is PicoTTL's default
everywhere.

**Code page 850** ("Multilingual", Latin-1) arrived with DOS 3.3
(1987), when IBM introduced switchable code pages for national
markets. CP437 could not spell many Western-European words - it lacks
almost all uppercase accented letters. CP850 sacrifices CP437's mixed
double/single box-drawing pieces and Greek/math symbols for the full
Latin-1 repertoire, and became the de facto standard for DOS in
Western Europe: Spanish, French, Portuguese, German and Nordic DOS
installations ran CP850 by default for over a decade. Crucially, IBM
kept every character shared by both pages at the SAME byte position,
so plain text - and TUIs restricted to pure single/double frames -
survive a page switch intact. Use CP850 when reproducing
Western-European DOS software or displaying international text; use
CP437 for everything else.

### Differing positions (normative inventory)

47 of the 256 positions render a different character under CP850.
Four of them re-home characters CP437 already had elsewhere: `¢`
(0x9B->0xBD), `¥` (0x9D->0xBE), `¶` (0x14, also at 0xF4) and `§` (0x15,
also at 0xF5). The remaining 43 introduce characters absent from
CP437 (CP437 rendering -> CP850 rendering):

| Pos | 437->850 | Pos | 437->850 | Pos | 437->850 | Pos | 437->850 |
|-----|---------|-----|---------|-----|---------|-----|---------|
| 9B | ¢->ø | C7 | ╟->Ã | DD | ▌->¦ | EC | ∞->ý |
| 9D | ¥->Ø | CF | ╧->¤ | DE | ▐->Ì | ED | φ->Ý |
| 9E | ₧->× | D0 | ╨->ð | E0 | α->Ó | EE | ε->¯ |
| A9 | ⌐->® | D1 | ╤->Ð | E2 | Γ->Ô | EF | ∩->´ |
| B5 | ╡->Á | D2 | ╥->Ê | E3 | π->Ò | F0 | ≡->SHY |
| B6 | ╢->Â | D3 | ╙->Ë | E4 | Σ->õ | F2 | ≥->‗ |
| B7 | ╖->À | D4 | ╘->È | E5 | σ->Õ | F3 | ≤->¾ |
| B8 | ╕->© | D5 | ╒->ı | E7 | τ->þ | F7 | ≈->¸ |
| BD | ╜->¢ | D6 | ╓->Í | E8 | Φ->Þ | F9 | ∙->¨ |
| BE | ╛->¥ | D7 | ╫->Î | E9 | Θ->Ú | FB | √->¹ |
| C6 | ╞->ã | D8 | ╪->Ï | EA | Ω->Û | FC | ⁿ->³ |
|    |     |    |     | EB | δ->Ù |    |     |

Note that all pure single-line and pure double-line box characters
survive; only the mixed single/double junction pieces were sacrificed.

### Provenance and the 9th-column rule

The CP850 datasets are generated from the CP850 8×14 and 8×8 fonts of
IBM PC-DOS 2000's codepage file (`EGA.CPI` lineage). CPI letterforms
differ subtly from the EGA/VGA ROM even in the ASCII range; this is
**authentic** - selecting CP850 on real DOS replaced all 256 glyphs,
and users saw exactly this rendition. The MDA 9th-column replication
rule remains position-based (codes 0xC0-0xDF), exactly like the
hardware circuit; IBM drew the CP850 letters that live in that range
(ã, Ã, ð, Ð, Ê, Ë, È, Í, Î, Ï, Ì) with pixel column 8 clear precisely
so they survive the replication - verified on this dataset (only 0xD5,
dotless i, touches column 8).

Example 44 (`examples/44_code_pages`) demonstrates both the
renderer-level (`Text`) and terminal-level (`Terminal`) switching
paths on MDA hardware.

## 15. The reference connector

PicoTTL's canonical physical interface is the **EGA connector** - the
same position IBM took in 1984: the EGA card's DE-9 was designed as a
superset that drove MDA monitors (5151), CGA monitors (5153) and EGA
monitors (5154) without rewiring. Every example, demonstration and
diagnostics application (with one documented exception below) uses
this single wiring, so monitors can be swapped freely:

| GPIO | DE-9 | MDA monitor | CGA monitor | EGA monitor |
|------|------|-------------|-------------|-------------|
| 2  | 8 | HSYNC | HSYNC | HSYNC |
| 3  | 9 | VSYNC | VSYNC | VSYNC |
| 16 | 5 | - | Blue | Primary Blue |
| 17 | 4 | - | Green | Primary Green |
| 18 | 3 | - | Red | Primary Red |
| 19 | 7 | **VIDEO** | (reserved) | Secondary Blue |
| 20 | 6 | **INTENSITY** | Intensity | Secondary Green (I) |
| 21 | 2 | (GND inside monitor) | (GND inside monitor) | Secondary Red |
| GND | 1 | GND | GND | GND |

GPIO 4 remains the internal display-enable handshake and is never
wired to the monitor.

Rules and rationale:

- **MDA applications use GPIO 19/20** (DE-9 pins 7/6) - exactly the
  pins through which a real EGA card drove a 5151. The RP2350
  consecutive-pin constraint holds (VIDEO = 19, INTENSITY = 19 + 1).
- **CGA monitors on the reference connector are driven by
  `EgaDisplayDevice`'s CGA modes** (`IbmCga640`/`IbmCga320` with a
  Packed8 framebuffer and `ega::kCga200Palette`, which places
  intensity on Secondary Green = pin 6 and keeps bits 3/5 low -
  Example 38 is the reference), reproducing precisely how IBM's EGA
  hardware served CGA monitors. Demonstration 97 and Example 43 follow
  this pattern: normal applications target the reference connector.
- **`CgaDisplayDevice` implements the authentic standalone CGA
  adapter** (B,G,R,I on four consecutive GPIOs, intensity on
  GPIO 19 -> pin 6). Its examples (25-33) and diagnostics application
  keep that genuine pinout - they are the CGA backend's electrical
  contract and validated regression surface, primarily for historical
  accuracy and users building a dedicated CGA card. On the reference
  connector their bright colors are lost (pin 6 is fed from GPIO 20);
  running them faithfully requires moving the single pin-6 wire from
  GPIO 20 to GPIO 19.
- **Ground goes to DE-9 pin 1 only.** On MDA and CGA monitors pin 2 is
  internally ground, but on the reference connector it carries GPIO 21
  (Secondary Red). Connect the monitor signal wires directly, except for
  **470 ohm series resistors on HSYNC and VSYNC**. The VSYNC resistor is
  empirically required for noise immunity; the same value on HSYNC keeps
  the sync wiring consistent. On the EGA reference connector pin 2 is a
  signal, not ground, so an MDA/CGA application must not connect Pico GND
  to that pin.
- The monitor inputs are specified as **+5 V TTL**, but the **+3.3 V** GPIO
  levels from the Raspberry Pi Pico are detected reliably by the monitors
  used with PicoTTL. For a more serious or production-oriented project,
  use buffers or level translators to raise the Pico's 3.3 V signals to
  +5 V before they reach the monitor.
- Scan rates still differ: 350-line modes must not be fed to a
  15.7 kHz CGA monitor and vice versa. The reference connector unifies
  *wiring*, not timing - select the application matching the attached
  monitor.

## Appendix A - Current RP2350 implementation (informative, not binding)

Everything in this appendix is an **implementation detail** of today's
RP2350 backend. Future implementations are free to use entirely different
techniques (different PIO programs, CPU-assisted generation, other
peripherals) without any change to the public API.

- The backend compiles a mode's raster description into a static table of
  32-bit segments (sync levels + duration, three segments per line) held
  in a fixed-size member of the device object.
- A PIO state machine running at the pixel clock consumes the table; two
  chained DMA channels replay it forever (data channel streams, control
  channel rewinds). The raster therefore runs with zero CPU involvement
  and no interrupts.
- The PIO clock divider is derived from the application-chosen system
  clock and snapped to an integer when the pixel clock error stays below
  1 % (e.g. 130 MHz ÷ 8 = 16.25 MHz vs canonical 16.257 MHz), eliminating
  fractional-divider jitter.
- HSYNC and VSYNC must be adjacent GPIOs (a constraint of consecutive PIO
  OUT pins) - validated at configuration time.
- The pixel path uses a second state machine synchronized to the sync
  generator through an internal display-enable GPIO; that design
  intentionally avoids embedded command formats (see design
  principles).

## Appendix B - IBM MDA raster reference

| Parameter | Value |
|---|---|
| Pixel clock | 16.257 MHz canonical |
| Horizontal | 720 active + 18 fp + 135 sync + 9 bp = 882 px, 18.43 kHz, HSYNC active-high |
| Vertical | 350 active + 0 fp + 16 sync + 4 bp = 370 lines, ~50 Hz, VSYNC active-low |
