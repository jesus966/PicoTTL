# Changelog

All notable changes to PicoTTL are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html)
with one deliberate strengthening: the public API is **frozen from the
first release** - additive evolution only, regardless of major version
(see [docs/api_evolution.md](docs/api_evolution.md)).

## [1.0.0] - 2026-07-24

Initial public release preparation. The milestones below reconstruct
the project's pre-release history from the repository.

### Added

- **Architecture and MDA foundation** (2026-07-12/13)
  - Layered architecture with a frozen public API
    (`Display`, `DisplayDevice`, `DisplaySurface`, `Graphics`,
    `VideoMode`) and the PIO + DMA scanline engine: zero CPU per
    scanline after `begin()`.
  - IBM MDA backend (720×350 @ 50 Hz, 18.43 kHz), validated on a real
    IBM 5151.
  - Packed pixel formats (1/2/4/6/8 bpp), `PackedFramebuffer<N>`,
    intensity support.
  - Examples 01–15: sync, pixel path, graphics primitives, bitmaps,
    deterministic stress patterns.
- **Text subsystem** (2026-07-13)
  - `Font` resource + `fonts::kIbmMda9x14` generated from an IBM
    character ROM dump (9th-column hardware rule baked in).
  - `Text` (cell-grid renderer with attributes: bright, underline,
    inverse), `Terminal` (cell buffer, cursor overlays, blink,
    auto-scroll, injected time), `AnsiTerminal` (incremental
    VT100-subset parser with backend-independent palette mapping).
  - Examples 16–24 including a USB CDC ANSI serial terminal.
- **Host backend and automated regression suite** (2026-07-13)
  - `host::HostDisplayDevice`: every mode and format in memory.
  - Dependency-free test framework; the suite now counts 56 tests.
- **CGA backend** (2026-07-14)
  - `IbmCga640` / `IbmCga320` (60 Hz, RGBI), validated on a Samtron
    SC-431E; recommended 142.8 MHz system clock (hardware-diagnosed
    vertical-capture fix); `cga::Colors` with the historical palettes;
    `fonts::kIbmCga8x8`.
  - Examples 25–33.
- **CRT diagnostics applications** (2026-07-14)
  - Per-family diagnostics apps (MDA, CGA, later EGA) sharing a
    pattern library; BOOTSEL-driven navigation; official validation
    tooling for real CRTs.
- **EGA backend** (2026-07-14)
  - `IbmEga350` (640×350 @ 60 Hz, 64 colors, 6-wire rgbRGB), Packed8
    scanout, CGA-compatible 200-line modes on the same device;
    `ega::Colors` incl. the power-on palette and the 200-line palette;
    `fonts::kIbmEga8x14` (verified byte-identical to the IBM VGA 8×14
    ROM font). Examples 34–42.
- **Host image display** (2026-07-17)
  - Example 43: PC image -> USB CDC -> framebuffer -> monitor, with a
    Python host (Pillow/NumPy/pySerial), aspect-correct scaling and
    ordered dithering. Includes an engine fix for cross-PIO sync-pin
    re-muxing in multi-device applications.
- **Showcase demonstrations** (2026-07-19)
  - Demonstrations 96 (MDA), 97 (CGA), 98 (EGA): the official,
    historically authentic capability showcases, with a shared
    `demo_common` utility library (dithering, transitions, CP437 UI,
    banner lettering).
- **IBM code pages** (2026-07-19)
  - `CodePage` metadata, `FontFamily`, `Text`/`Terminal`
    `setCodePage()` with authentic `MODE CON CP SELECT` semantics;
    complete CP850 datasets for all three font families, generated
    from IBM PC-DOS 2000 CPI data; Example 44.
- **MDA phosphor rain** (2026-07-24)
  - Example 45: asynchronous Matrix-style character streams with bright
    heads and normal tails, designed to use the physical afterglow of
    IBM 5151-class medium/high-persistence phosphor monitors.
- **Reference connector** (2026-07-20)
  - The IBM EGA DE-9 adopted as the canonical physical interface (one
    wiring for all monitors); MDA applications moved to the EGA
    card's video/intensity pins; demonstration 97 and Example 43
    migrated to `EgaDisplayDevice` CGA-compatible modes;
    `CgaDisplayDevice` preserved as the authentic standalone CGA
    adapter implementation.
- **Release hardening** (2026-07-21/23)
  - Hostile pre-release review of the whole stack. Fixes: `AnsiTerminal`
    CSI parameter overflow handling (excess parameters discarded whole,
    values saturated), `Graphics::drawLine` 64-bit arithmetic for
    extreme legal coordinates, `printf` error-return checks, a
    diagnostics pattern guard for degenerate surface heights. Two new
    hostile-input tests (suite at 56).
  - Continuous integration workflow (host tests + firmware build with
    UF2 artifacts), `.editorconfig`, issue/PR templates.
  - `THIRD_PARTY_LICENSES.md`: full provenance documentation for the
    font datasets and build imports; the example sprite replaced with
    an original design.
- **Embedding support** (2026-07-24)
  - The repository can be added to an existing Pico SDK project with
    `add_subdirectory()` after `pico_sdk_init()`: only the `picottl`
    library target is created (no examples/diagnostics), with clear
    configuration errors for wrong ordering or non-RP2350 platforms.
    New [docs/integration.md](docs/integration.md) covers submodule,
    vendored-copy and FetchContent integration; the stable build
    interface (target + include spellings) documented in
    [docs/api_evolution.md](docs/api_evolution.md).
  - README media: real CRT photographs of the reference wiring and the
    three showcase demonstrations.

[1.0.0]: https://github.com/jesus966/PicoTTL/releases/tag/v1.0.0
