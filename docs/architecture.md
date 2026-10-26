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

Frequency queries describe nominal mode metadata, not measured output or the
RP2350 engine's effective programmed rate. `Display::refreshRateMilliHz()` is
valid after a successful `begin()` and remains mode metadata after `end()`.
A failed operation can also leave the last successful mode selected; the rate
is not evidence that output is running or configuration is valid. Use
`isActive()` to check running state. `DisplayDevice` additionally permits the
backend's default-mode metadata before the first successful `begin()`.


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

`Display` keeps one drawing context for its lifetime. An accepted
`Display::setFramebuffer()` immediately binds that context to the backend's
current surface; detaching binds it to null and makes drawing inert, without
requiring another `begin()`. Rejected framebuffer changes leave the binding
unchanged. `end()` preserves it, and a failed `begin()` leaves it unchanged;
a successful `begin()` also synchronizes it with the backend's surface.
Mode compatibility is still validated by `begin()`, independently of CPU drawing.

References to `display.graphics()` retain their identity, and `Text` using
that context follows its binding. After an accepted replacement or detachment,
the retired framebuffer can be destroyed once no other consumer uses it.
Independently constructed `Graphics` contexts still require their own surface
to remain alive. Make framebuffer changes through `Display::setFramebuffer()`
when using its drawing context: direct backend changes bypass this synchronization.
No ownership is transferred, and framebuffer memory must outlive its wrapper.

### Concurrency and multicore ownership

PicoTTL supplies no object locks or atomic state fields and does not promise
thread-safe shared-object mutation. Prefer one owner core for a display and
its rendering contexts. Lifecycle methods can run on either core after a
synchronized ownership handoff; they must not run concurrently. The same
rules apply to interrupt reentrancy, not just the second core. These are caller
obligations, not runtime ownership checks.

| State or resource | Access and ownership contract |
|---|---|
| Immutable mode/color/font data and pure value helpers | Concurrent reads are safe while data is available. Publish shared objects before use; do not mutate or destroy them concurrently. |
| Framebuffer descriptor/geometry | Read-sharing requires stable lifetime and binding. `data()` returns a borrowed pointer, not a lock or lease. |
| Pixel storage | Serialize overlapping CPU reads/writes. CPU writes to live scanned SRAM are intended, with possible tearing; DMA is not a C++ caller and does not acquire a CPU lock. |
| `Display`, devices and engine | Serialize begin/end, configure/start/stop, source changes, destruction and queries that race mutation. Coordinate shared PIO/DMA/GPIO/clock resources across instances and non-PicoTTL users. |
| `Graphics`, `Text`, `Terminal`, `AnsiTerminal` | Bindings, cursor/style/font/parser state, cells and rendering have no internal synchronization. Separate contexts may still alias the same storage. |
| Timing tables and DMA control-source words | Owned by the device/engine and read asynchronously by DMA. Do not access or change them externally; stop/reconfigure through the serialized lifecycle. |
| Application static devices/buffers | Static lifetime supplies neither exclusive ownership nor synchronization. The shipped applications do not launch core 1. |

Pixels at different coordinates are not necessarily different memory
locations. Mono1/Packed2/Packed4 share bytes; Packed6 also has fields crossing
byte boundaries (it is not scanned directly by the RP2350 backends). Two
read-modify-write operations can both read byte `00`, compute mono pixel
updates `80` and `40`, then store `80` followed by `40`, losing the first pixel.
CPU data races are undefined C++, regardless of machine load/store atomicity.
Four-byte DMA alignment does not make these operations atomic. Packed8 direct
pixel stores use separate bytes for separate pixels, but that alone does not
establish a general concurrent-drawing contract. Any advanced partitioning
must prove disjoint **byte read/write footprints**, including padding and all
bulk operations; PicoTTL does not validate such a partition.

`clear`/`fill`, rectangle/bitmap/text drawing, inversion and row copying can
span or read multiple pixels. `memmove` handles overlap within one call, not
concurrent calls. Text scrolling, terminal cursor/blink updates and ANSI
processing also mutate shared state. Serialize the complete operation, not
just individual pixel stores. Read-only queries are safe only while the
state/data they read is stable; `isActive()` is not an atomic ready flag.

Rendering while PIO/DMA scan SRAM is separate from CPU/CPU concurrency. A
frame can contain old and new rows/pixels, and DMA may already have prefetched
data when a CPU store completes. A CPU mutex does not pause DMA or provide an
atomic image update. PicoTTL supplies no frame-publication, cache-maintenance
or cross-core visibility protocol. Use appropriate SDK queues/mutexes or
release/acquire synchronization for CPU ownership and data handoff; plain
flags and `volatile` are insufficient. External memory or cached aliases need
an application-specific visibility/ordering assessment. No extra barrier is
introduced into drawing paths, and host tests do not establish RP2350 ordering.

SDK hardware-claim locks protect individual claim/program-allocation calls,
not PicoTTL's check-then-claim configuration transaction. Concurrent callers
can both observe a resource as free before one claims it; the other's SDK
claim may panic. Serialize resource management, keep hardware assignments
exclusive and preserve the shared PIO GPIO base. State checks and SDK locks
are not substitutes for this contract. Do not reenter these operations from
an ISR; a blocking lock can deadlock against interrupted foreground work.

Before retiring a framebuffer: pause CPU drawing/readers and acknowledge their
quiescence through a synchronized channel; call `end()` and accept detachment
through `Display::setFramebuffer(nullptr)`; retire independently bound contexts
and any other consumers; then destroy the old descriptor/storage. The existing
stop sequence retires DMA before return. `end()` alone preserves bindings and
resource claims, so it is not permission to free still-referenced objects.
Do not allow another core to restart output during retirement.

A recommended multicore pattern keeps all PicoTTL calls on core 0. Core 1
computes independent data and transfers work through an application-owned SDK
queue; core 0 consumes it and draws while PIO/DMA continue asynchronously.
Define payload ownership until the receiver acknowledges completion. Another
single rendering core is possible after publication of stable bindings, but
lifecycle changes require it to pause/acknowledge first. Simultaneous writers
need external serialization; this documentation does not make a race safe.

### Flash access and BOOTSEL

PicoTTL contains no flash erase/program path. The diagnostic BOOTSEL helper
executes from RAM, masks interrupts on the **calling core**, temporarily floats
flash chip select, reads the pin and restores it. It is not a flash write.
It does not park the other core, stop flash-reading DMA/XIP streaming, or make
other flash code/data unavailable to those users. Call it only when every
other flash accessor is excluded for the entire interval. The stock diagnostics
are single-core with SRAM scanout; adding a worker running flash code changes
that assumption even if the worker never calls PicoTTL. Flash-resident fonts,
bitmaps and rendering code also need flash access.

An application adding flash writes must establish the SDK's appropriate flash
safe-execution/other-core lockout protocol, including required per-core setup,
and account separately for DMA/other bus masters reading flash. SRAM-only
PIO/DMA scanout has no CPU service requirement, but continuing it safely still
requires resident buffers/control words and hardware validation under load.
PicoTTL adds no automatic flash lockout; local interrupt masking alone is not
a multicore exclusion mechanism.

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
  simplify the API (e.g. the timing table embedded in
  `MdaDisplayDevice`), provided they are documented and visible in the
  map file.

Device objects contain their timing tables, so they are several kilobytes
rather than small handles. Their storage duration must be included in the
application's memory budget. A device constructed as an automatic variable
occupies its caller's stack for its entire lifetime, including while DMA
is generating video. See the backend memory reference below for table sizes.

For a single device used throughout a firmware's permanent `main()` loop,
function-local static storage keeps the table out of the stack while
constructing the object at first use, after the application prepares its
clock and configuration. Each declaration owns a separate instance; this
does not make the device shared between cores or its operations thread-safe.
Do not apply this policy to reentrant functions, short-lived devices or
factories that require a new instance on each call.

The configuration is copied during construction, but framebuffer descriptors
and pixel storage remain caller-owned and must outlive every active use.
A static device must not retain pointers into a scope that has returned.
The shipped single-core applications keep any automatic framebuffer
descriptor inside their non-returning `main()`; its pixel storage is static.
`end()` still stops output, while destruction of a static device belongs to
program termination rather than block exit. Retain automatic or other
explicitly owned storage where deterministic scoped destruction is required,
and budget enough stack for the complete call chain and interrupts.
Changing a DMA buffer's address can also change SRAM-bank contention;
validate streaming under the application's CPU/DMA load on hardware.

## 8. Hardware resource policy

The internal RP2350 scanline engine uses invalidating configuration:
`configure()` first stops its owned hardware and invalidates the previous
configuration. A failure leaves the engine stopped; `start()` rejects it
until a subsequent `configure()` succeeds. Error exits also stop hardware
claimed during that attempt: SDK resource claims do not reset its registers.
Hardware parameters (including pixel pin spans and resource conflicts within
one configuration) are validated before acquisition. GPIO mux, pad and output
direction setup is deferred until resource acquisition and PIO program
installation succeed. Resource exhaustion or a rejected configuration therefore
does not newly configure the pins. Stopping previously running output still
drives its configured idle levels; previous pin state is not restored.
Timing-table contents and PIO programs are not rolled back. Resources acquired
before a later acquisition/program failure remain owned for a retry and are
released on engine destruction. `end()` stops output but preserves ownership
for reuse; it does not release pins to their former users. Callers must keep
the timing buffer and pixel storage alive as required by the engine, serialize
lifecycle calls, and must not treat a rejected configuration as permission to
reuse channels or state machines still owned by that instance.

The framework **never silently claims hardware**. The application always
decides, via the backend `Config`:

- which PIO block and state machine
- which DMA channels
- which GPIO pins

The framework validates the choices, claims exactly those resources,
fails gracefully (returns `false`) if any is unavailable, and releases
them in the backend destructor. Global machine state (system clock,
voltage) is never touched: clock policy belongs to the application.

GPIO numbers are absolute, not relative to a PIO window. RP2350A has GPIO
0..29; RP2350B has GPIO 0..47. The target board may expose fewer pins or
reserve some for onboard hardware. Pico 2 uses RP2350A, so it cannot expose
GPIO 32..47. Its onboard connections also make package availability alone
insufficient when selecting application pins.

Each PIO block shares one 32-pin GPIO window across all its state machines:
base 0 addresses GPIO 0..31, and base 16 addresses GPIO 16..47. All sync,
internal display-enable and active pixel pins must fit the **current** window
and the target package. Consecutive groups crossing GPIO 31/32 work at base
16, but combinations needing both GPIO 0..15 and GPIO 32..47 cannot use one
PIO block. Existing ascending/consecutive pin-group requirements still apply.

The backend never changes GPIOBASE, including during retry, stop or destruction.
For high GPIOs, build for an RP2350B board with SDK `PICO_PIO_USE_GPIO_BASE=1`
(the default for that package). Before loading programs or starting any PIO
users, the application must select base 16 using SDK
`pio_set_gpio_base(pio_get_instance(config.pioInstance), 16)` and check its
return code. Keep that base stable until **all** users have released their
resources; `end()` alone is insufficient. A window mismatch is rejected
before acquisition or GPIO setup. Compatible foreign state machines can share
the block without being relocated.

SDK pin-configuration functions receive absolute pin numbers and perform the
base translation. Idle levels use the SDK's absolute 64-bit mask operation,
which translates to the selected PIO window without out-of-range 32-bit shifts.
The host recorder checks mapping and lifecycle logic; RP2350B electrical output,
board routing and CRT compatibility still require physical validation.


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
  clock and snapped to an integer when the pixel clock error is within
  1 % (e.g. 130 MHz ÷ 8 = 16.25 MHz vs canonical 16.257 MHz), eliminating
  fractional-divider jitter in the timing state machine.
  Configuration requires positive nominal pixel and SDK-reported system
  frequencies, with their exact ratio in the PIO divider range [1, 65536].
  The divider has 16 integer and 8 fractional bits; integer/fraction 0/0
  represents 65536. Each timing-table segment takes exactly its requested
  number of timing-SM cycles; every packed pixel program takes two pixel-SM
  cycles per pixel, independent of depth. Consequently, the **encoded timing
  divider must equal twice the encoded pixel divider**.
  With a framebuffer, integer snapping is applied first as above, then the
  pixel divider is quantized once using the SDK rounding policy (nearest by
  default in SDK 2.3.0; truncation if configured). The timing divider is twice
  that fixed-point result. Both registers are written as explicit integer/
  fraction fields, avoiding independent rounding. Thus the timing divider
  has 1/128 resolution on the pixel path; signal-only output retains 1/256.
  Both selected dividers must remain in [1, 65536], so pixel output requires
  a timing divider of at least 2 after snapping. The 65536 endpoint still
  uses integer/fraction 0/0. Integer-snapped clocks remain unchanged; otherwise
  the timing frequency may move slightly to match the quantized pixel rate.
  Unsnapped pixel quantization contributes less than 0.4% frequency error
  over the supported divider range (less than 0.2% with nearest rounding).
  Unrepresentable clocks are rejected before PIO/DMA setup;
  as with other configuration failures, the engine remains stopped and
  requires a successful configuration before starting. The application
  must keep the system clock stable after configuration and during output.
  `start()` enables the two state machines together with divider phases
  aligned. Display-enable gates pixel output on each visible line; this
  provides per-line synchronization, not compensation for mismatched divider
  rates. Coherent dividers remove the rate discrepancy but do not eliminate
  fractional-cycle jitter, instruction latency or FIFO stalls. Physical
  signal and pixel-position validation remains necessary.
- HSYNC and VSYNC must be adjacent GPIOs (a constraint of consecutive PIO
  OUT pins) - validated at configuration time.
- The pixel path uses a second state machine synchronized to the sync
  generator through an internal display-enable GPIO; that design
  intentionally avoids embedded command formats (see design
  principles).

### Custom timing constraints

Arithmetic validity, engine representability and monitor compatibility are
separate concerns. `VideoTiming` helpers describe nominal arithmetic; the
RP2350 engine checks its own encoding constraints, without enforcing a CRT
modeline or monitor acceptance range.

The active horizontal span, HSYNC width and back porch each require 3..65535
cycles: each is emitted as a segment whose delay is `span - 3`. The horizontal
front porch may be zero (no segment), or 3..65535. Ordering is fixed by the
fields, so HSYNC cannot overlap the active span. Only `ActiveHigh` and
`ActiveLow` sync polarities are accepted; unknown enum values are rejected.

Each vertical interval may be 0..65535 lines, but the total must be nonzero.
Zero intervals are skipped, including zero active or VSYNC intervals in
signal-only configurations. This can produce a constant VSYNC or no visible
window; it remains executable by the engine and is not a promise of usable
CRT video. A table period does not imply VSYNC edges. A positive pixel source
cannot match zero active height. Zero vertical porches and single-line signal frames do not need special encoding.

The field types bound each total to 262140. Three or four words per line
require at most 1048560 timing words, within the RP2350 normal DMA count field;
the caller must provide that storage. The 29-bit segment delay is wider than
any span representable by the public horizontal fields.

Pixel streams additionally require whole bytes per line, an exact visible-frame
size and four-byte source alignment/size. Their word count must not exceed the
SDK's 28-bit normal DMA count (`0x0fffffff` transfers, or 1073741820 bytes).
Larger values would occupy DMA mode bits and are rejected before table writes
or hardware acquisition. This encoding bound does not guarantee that the board
has enough addressable memory; buffer capacity and lifetime remain the caller's
responsibility. Failed reconfiguration still stops previous output and retains
its established resource ownership.

### Nominal and programmed frequencies

`VideoTiming::pixelClockHz` is nominal. Its helpers return
`floor(pixelClockHz / hTotal())` Hz and
`floor(1000 * pixelClockHz / (hTotal() * vTotal()))` millihertz, subject to their
existing zero-result contract. RP2350 and host devices expose that nominal
refresh through `DisplayDevice` and `Display`; no existing public query reports
an effective clock. These descriptors are not changed by hardware configuration.

For a successfully configured RP2350 engine, let `S` be the system frequency
used during configuration and `Q` the timing divider in units of 1/256. Decode
`Q = 256 * INT + FRAC` from CLKDIV; integer/fraction 0/0 means `Q = 16777216`
(divider 65536), not zero. The expected average rates are:

```text
timing-SM clock = S * 256 / Q
horizontal Hz   = S * 256 / (Q * hTotal())
vertical Hz     = S * 256 / (Q * hTotal() * vTotal())
```

Widen operands before integer multiplication and retain precision until the
final division; do not derive vertical refresh from an already truncated
horizontal result. Each timing-table segment executes its encoded delay plus
three cycles, exactly its requested horizontal span. All visible and blanking
lines are emitted with no additional programmed per-line/per-frame cycles.
Pixel depth does not change those counts. With pixels enabled, the pixel SM
runs at twice the timing-SM frequency; without pixels, horizontal and vertical
rates remain meaningful and there is no active pixel-SM frequency.

These are programmed averages assuming a stable system clock and an unstalled
timing stream, not electrical measurements. Fractional-divider jitter, FIFO
stalls and oscillator tolerance require separate hardware assessment. The
engine retains its selected divider, but not a snapshot of the system clock;
a failed configuration can retain attempted/stale values, and stopped output
has no generated rate. Such state must not be interpreted as a valid effective
frequency sample. Changing the system clock requires reconfiguration.

For example, CGA640 at 130 MHz snaps to divider 9 (pixel-SM divider 4.5).
Nominal refresh is 59.922743 Hz, exposed as 59922 mHz after truncation; the
programmed refresh is approximately 60.451170 Hz. This difference is caused by
integer snapping, not a measurement or a clock-ratio error. Diagnostic Status
and Timings screens likewise show nominal values: compare scope measurements
against the configured-divider calculation, not nominal values within crystal
tolerance alone.

## Appendix B - IBM MDA raster reference

| Parameter | Value |
|---|---|
| Pixel clock | 16.257 MHz canonical |
| Horizontal | 720 active + 18 fp + 135 sync + 9 bp = 882 px, 18.43 kHz, HSYNC active-high |
| Vertical | 350 active + 0 fp + 16 sync + 4 bp = 370 lines, ~50 Hz, VSYNC active-low |
