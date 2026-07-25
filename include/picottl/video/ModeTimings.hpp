// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// ModeTimings.hpp
// Canonical raster timings for each VideoMode.
//
// BACKEND SPI - used by display drivers, not by applications.
// Adding a new standard means adding its VideoTiming entry here (or in
// the new driver) plus the VideoMode value; nothing else changes.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "picottl/VideoMode.hpp"
#include "picottl/video/VideoTiming.hpp"

namespace picottl {

namespace modes {

/// IBM MDA: 720x350 visible, 16.257 MHz pixel clock, 18.432 kHz horizontal,
/// ~50 Hz vertical. HSYNC is active-high, VSYNC is active-low.
///
/// Derived from the original 6845 register set used by the IBM MDA:
///   horizontal total 98 chars x 9 px = 882, hsync at char 82 width 15 chars;
///   vertical total 370 lines, vsync at line 350 width 16 lines.
inline constexpr VideoTiming kIbmMda{
    16'257'000, // pixelClockHz
    720,        // hActive
    18,         // hFrontPorch
    135,        // hSyncWidth
    9,          // hBackPorch
    350,        // vActive
    0,          // vFrontPorch
    16,         // vSyncWidth
    4,          // vBackPorch
    SyncPolarity::ActiveHigh, // hSyncPolarity
    SyncPolarity::ActiveLow,  // vSyncPolarity
};

} // namespace modes

namespace modes {

/// MDA overscan: a BACKEND-DEFINED mode that intentionally exceeds the
/// historical IBM raster while keeping the monitor perfectly
/// synchronized. Its geometry is DERIVED FROM CONSTRAINTS - it is not
/// another "magic" standard:
///   - pixel clock, sync pulse widths and horizontal/vertical totals
///     (hence both sync frequencies) are exactly IBM MDA's;
///   - the active width is the largest byte-aligned span (whole bytes
///     at 1/2/4/8 bpp - DMA friendliness) that leaves the minimum
///     horizontal porches the raster engine can emit;
///   - the active height leaves a small CRT retrace margin after vsync.
/// If hardware validation later allows different margins, only these
/// constraint constants change - the mode identity and every API stay
/// untouched. Further overscan variants (safe/maximum/experimental)
/// are added as additional additive modes.
namespace overscan_detail {

inline constexpr std::uint16_t kMinHorizontalPorch = 3;    ///< Engine segment minimum.
inline constexpr std::uint16_t kVerticalRetraceMargin = 2; ///< CRT safety lines after vsync.
inline constexpr std::uint16_t kActiveAlignment = 8;       ///< Whole bytes at any depth.

inline constexpr std::uint16_t kActiveWidth = static_cast<std::uint16_t>(
    (kIbmMda.hTotal() - kIbmMda.hSyncWidth - 2u * kMinHorizontalPorch) /
    kActiveAlignment * kActiveAlignment);
inline constexpr std::uint16_t kBlanking = static_cast<std::uint16_t>(
    kIbmMda.hTotal() - kIbmMda.hSyncWidth - kActiveWidth);
inline constexpr std::uint16_t kBackPorch =
    kBlanking / 2 > kMinHorizontalPorch ? static_cast<std::uint16_t>(kBlanking / 2)
                                        : kMinHorizontalPorch;
inline constexpr std::uint16_t kFrontPorch =
    static_cast<std::uint16_t>(kBlanking - kBackPorch);
inline constexpr std::uint16_t kActiveHeight = static_cast<std::uint16_t>(
    kIbmMda.vTotal() - kIbmMda.vSyncWidth - kVerticalRetraceMargin);

} // namespace overscan_detail

inline constexpr VideoTiming kMdaOverscan{
    kIbmMda.pixelClockHz,
    overscan_detail::kActiveWidth,
    overscan_detail::kFrontPorch,
    kIbmMda.hSyncWidth,
    overscan_detail::kBackPorch,
    overscan_detail::kActiveHeight,
    0, // vFrontPorch: vsync directly after active, like the IBM raster.
    kIbmMda.vSyncWidth,
    overscan_detail::kVerticalRetraceMargin,
    kIbmMda.hSyncPolarity,
    kIbmMda.vSyncPolarity,
};

// The monitor must not notice the difference in synchronization.
static_assert(kMdaOverscan.hTotal() == kIbmMda.hTotal(),
              "overscan must preserve the horizontal sync frequency");
static_assert(kMdaOverscan.vTotal() == kIbmMda.vTotal(),
              "overscan must preserve the vertical sync frequency");
static_assert(kMdaOverscan.hActive % 8 == 0,
              "overscan active width must fill whole bytes");
static_assert(kMdaOverscan.hFrontPorch >= overscan_detail::kMinHorizontalPorch &&
                  kMdaOverscan.hBackPorch >= overscan_detail::kMinHorizontalPorch,
              "overscan porches must satisfy the engine segment minimum");
static_assert(kMdaOverscan.hActive >= kIbmMda.hActive &&
                  kMdaOverscan.vActive >= kIbmMda.vActive,
              "overscan never shrinks below the IBM raster");

/// IBM CGA, 640x200 raster: 14.318 MHz pixel clock (4x the NTSC color
/// subcarrier), 15.700 kHz horizontal, ~59.9 Hz vertical. Both syncs
/// are active-high.
///
/// Derived from the original 6845 register set used by the IBM CGA
/// BIOS for 80-column modes:
///   horizontal total 114 chars x 8 px = 912, displayed 80 chars,
///   hsync at char 90 width 10 chars;
///   vertical total 32 rows x 8 lines + 6 adjust = 262 lines,
///   displayed 200 lines, vsync at line 224 width 16 lines.
inline constexpr VideoTiming kIbmCga640{
    14'318'180, // pixelClockHz
    640,        // hActive
    80,         // hFrontPorch
    80,         // hSyncWidth
    112,        // hBackPorch
    200,        // vActive
    24,         // vFrontPorch
    16,         // vSyncWidth
    22,         // vBackPorch
    SyncPolarity::ActiveHigh, // hSyncPolarity
    SyncPolarity::ActiveHigh, // vSyncPolarity
};

/// IBM CGA, 320x200 raster: the same modeline with every horizontal
/// quantity halved (the CGA runs its 6845 at half character clock for
/// 40-column and 320-pixel modes). Sync frequencies are identical to
/// kIbmCga640, so a monitor never notices the difference.
inline constexpr VideoTiming kIbmCga320{
    7'159'090, // pixelClockHz (kIbmCga640's halved)
    320,       // hActive
    40,        // hFrontPorch
    40,        // hSyncWidth
    56,        // hBackPorch
    kIbmCga640.vActive,
    kIbmCga640.vFrontPorch,
    kIbmCga640.vSyncWidth,
    kIbmCga640.vBackPorch,
    kIbmCga640.hSyncPolarity,
    kIbmCga640.vSyncPolarity,
};

static_assert(kIbmCga640.hTotal() == 912 && kIbmCga640.vTotal() == 262,
              "CGA totals must match the 6845 register derivation");
static_assert(kIbmCga320.hTotal() * 2 == kIbmCga640.hTotal() &&
                  kIbmCga320.pixelClockHz * 2 == kIbmCga640.pixelClockHz,
              "the 320 raster is exactly the 640 raster at half pixel clock");
static_assert(kIbmCga320.lineFrequencyHz() == kIbmCga640.lineFrequencyHz(),
              "both CGA rasters share the same sync frequencies");
static_assert(kIbmCga640.hActive % 8 == 0 && kIbmCga320.hActive % 8 == 0,
              "CGA active widths must fill whole bytes");

/// IBM EGA, 640x350 raster: 16.257 MHz pixel clock (the same crystal
/// as MDA), 21.85 kHz horizontal, ~60 Hz vertical. HSYNC is
/// active-high; VSYNC is ACTIVE-LOW - the negative vertical sync is
/// how the 5154 Enhanced Color Display selects its 350-line mode
/// (positive VSYNC selects the CGA-compatible 200-line mode).
///
/// Anchored values (IBM Options & Adapters): pixel clock, 744-pixel
/// line (93 chars), 364-line frame, actives, polarities. The porch
/// splits are derived from the EGA CRTC register set and are subject
/// to oscilloscope validation (data-only to adjust; the Timings
/// diagnostic pattern is the companion tool).
///
/// The EGA card is also hardware-compatible with CGA timings: for
/// 200-line operation the EGA backend references kIbmCga640 and
/// kIbmCga320 directly (compatibility, not duplication).
inline constexpr VideoTiming kIbmEga350{
    16'257'000, // pixelClockHz
    640,        // hActive
    16,         // hFrontPorch
    64,         // hSyncWidth
    24,         // hBackPorch
    350,        // vActive
    0,          // vFrontPorch
    3,          // vSyncWidth
    11,         // vBackPorch
    SyncPolarity::ActiveHigh, // hSyncPolarity
    SyncPolarity::ActiveLow,  // vSyncPolarity
};

static_assert(kIbmEga350.hTotal() == 744 && kIbmEga350.vTotal() == 364,
              "EGA totals must match the IBM 350-line raster");
static_assert(kIbmEga350.pixelClockHz == kIbmMda.pixelClockHz,
              "EGA 350-line mode shares the MDA pixel crystal");
static_assert(kIbmEga350.hActive % 8 == 0,
              "EGA active width must fill whole bytes");

} // namespace modes

/// Returns the canonical timing for a video mode identifier.
constexpr const VideoTiming& timingFor(VideoMode mode) {
    switch (mode) {
        case VideoMode::MdaOverscan:
            return modes::kMdaOverscan;
        case VideoMode::IbmCga640:
            return modes::kIbmCga640;
        case VideoMode::IbmCga320:
            return modes::kIbmCga320;
        case VideoMode::IbmEga350:
            return modes::kIbmEga350;
        case VideoMode::IbmMda:
        default:
            return modes::kIbmMda;
    }
}

// The public geometry table (VideoMode.hpp) is intentionally separate
// from this timing catalog: the public header must not drag backend
// timing types into application code. These asserts keep the two
// tables consistent - the constraint-derived timing is authoritative,
// the public values are checked mirrors. Add a pair for every new mode.
static_assert(modeWidth(VideoMode::IbmMda) == modes::kIbmMda.hActive,
              "modeWidth(IbmMda) disagrees with the timing catalog");
static_assert(modeHeight(VideoMode::IbmMda) == modes::kIbmMda.vActive,
              "modeHeight(IbmMda) disagrees with the timing catalog");
static_assert(modeWidth(VideoMode::MdaOverscan) == modes::kMdaOverscan.hActive,
              "modeWidth(MdaOverscan) disagrees with the constraint-derived timing");
static_assert(modeHeight(VideoMode::MdaOverscan) == modes::kMdaOverscan.vActive,
              "modeHeight(MdaOverscan) disagrees with the constraint-derived timing");
static_assert(modeWidth(VideoMode::IbmCga640) == modes::kIbmCga640.hActive &&
                  modeHeight(VideoMode::IbmCga640) == modes::kIbmCga640.vActive,
              "modeWidth/Height(IbmCga640) disagree with the timing catalog");
static_assert(modeWidth(VideoMode::IbmCga320) == modes::kIbmCga320.hActive &&
                  modeHeight(VideoMode::IbmCga320) == modes::kIbmCga320.vActive,
              "modeWidth/Height(IbmCga320) disagree with the timing catalog");
static_assert(modeWidth(VideoMode::IbmEga350) == modes::kIbmEga350.hActive &&
                  modeHeight(VideoMode::IbmEga350) == modes::kIbmEga350.vActive,
              "modeWidth/Height(IbmEga350) disagree with the timing catalog");

} // namespace picottl
