// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// VideoMode.hpp
// Application-facing video mode identifiers.
//
// PUBLIC API - FROZEN. New modes may be added; existing values never change.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace picottl {

/// Curated catalog of the video modes known to the framework.
///
/// Standards and modes are separate concepts: a video STANDARD (MDA,
/// CGA, EGA) is a family implemented by one display backend;
/// a video MODE is one concrete raster configuration. One standard may
/// offer several modes - MdaOverscan joined IbmMda this way, and
/// further variants (e.g. an MdaFullRaster) are added as additive
/// values served by the same MDA backend. Backends advertise the modes
/// they support through DisplayDevice::supports(); begin() fails
/// explicitly for the rest.
///
/// IBM-exact timings are curated defaults, not an architectural limit:
/// the raster engine underneath is timing-agnostic, so non-standard,
/// experimental and diagnostic modes are first-class additions.
/// Existing enumerator values never change.
enum class VideoMode : std::uint8_t {
    IbmMda,      ///< IBM MDA standard mode, 720x350 @ ~50 Hz, 100% IBM compatible.
    MdaOverscan, ///< MDA-family backend-defined mode: the largest stable
                 ///< raster on an MDA monitor, derived from timing
                 ///< constraints (not a historical standard). Sync
                 ///< timing is identical to IbmMda. Further overscan
                 ///< variants are added as additional enumerators.
    IbmCga640,   ///< IBM CGA 640x200 raster @ ~60 Hz: the 80-column
                 ///< text and high-resolution graphics raster.
    IbmCga320,   ///< IBM CGA 320x200 raster @ ~60 Hz: the 40-column
                 ///< text and 4-color graphics raster. Same sync
                 ///< frequencies as IbmCga640 at half the pixel clock.
    IbmEga350,   ///< IBM EGA 640x350 raster @ ~60 Hz (21.85 kHz
                 ///< horizontal): the enhanced 350-line raster of the
                 ///< 5154 Enhanced Color Display. VSYNC is active-low
                 ///< (the monitor's 350-line mode select).
};

/// Visible width in pixels of a video mode. Usable in constant
/// expressions, e.g. to size framebuffer storage. Values for
/// backend-defined modes are mirrors of the constraint-derived timings,
/// bound to them by static_asserts in the timing catalog.
constexpr std::uint16_t modeWidth(VideoMode mode) {
    switch (mode) {
        case VideoMode::MdaOverscan:
            return 736;
        case VideoMode::IbmCga640:
        case VideoMode::IbmEga350:
            return 640;
        case VideoMode::IbmCga320:
            return 320;
        case VideoMode::IbmMda:
        default:
            return 720;
    }
}

/// Visible height in pixels of a video mode. Usable in constant
/// expressions, e.g. to size framebuffer storage.
constexpr std::uint16_t modeHeight(VideoMode mode) {
    switch (mode) {
        case VideoMode::MdaOverscan:
            return 352;
        case VideoMode::IbmCga640:
        case VideoMode::IbmCga320:
            return 200;
        case VideoMode::IbmEga350:
            return 350;
        case VideoMode::IbmMda:
        default:
            return 350;
    }
}

} // namespace picottl
