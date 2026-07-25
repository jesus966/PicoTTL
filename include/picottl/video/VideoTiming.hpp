// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// VideoTiming.hpp
// Platform-independent description of a raster video timing.
//
// A VideoTiming is a pure data object (a "modeline") that fully describes
// the geometry of one video mode: pixel clock, horizontal and vertical
// phases, and sync polarities. It contains no hardware knowledge and can
// be used, inspected and unit-tested on any platform.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace picottl {

/// Electrical polarity of a sync signal.
enum class SyncPolarity : std::uint8_t {
    ActiveHigh, ///< Sync pulse is a logic-high pulse.
    ActiveLow   ///< Sync pulse is a logic-low pulse.
};

/// Complete raster timing description for a TTL video mode.
///
/// All horizontal values are expressed in pixel clocks, all vertical
/// values in scanlines. A scanline is structured as:
///
///   [ active | front porch | sync pulse | back porch ]
///
/// and a frame as:
///
///   [ active lines | front porch lines | vsync lines | back porch lines ]
struct VideoTiming {
    std::uint32_t pixelClockHz;  ///< Nominal pixel clock in Hz.

    std::uint16_t hActive;       ///< Visible pixels per line.
    std::uint16_t hFrontPorch;   ///< Pixels between active video and hsync.
    std::uint16_t hSyncWidth;    ///< Hsync pulse width in pixels.
    std::uint16_t hBackPorch;    ///< Pixels between hsync and next active video.

    std::uint16_t vActive;       ///< Visible scanlines per frame.
    std::uint16_t vFrontPorch;   ///< Lines between active video and vsync.
    std::uint16_t vSyncWidth;    ///< Vsync pulse width in lines.
    std::uint16_t vBackPorch;    ///< Lines between vsync and next active video.

    SyncPolarity hSyncPolarity;  ///< Polarity of the horizontal sync pulse.
    SyncPolarity vSyncPolarity;  ///< Polarity of the vertical sync pulse.

    /// Total pixel clocks per scanline (visible + blanking).
    constexpr std::uint32_t hTotal() const {
        return static_cast<std::uint32_t>(hActive) + hFrontPorch + hSyncWidth + hBackPorch;
    }

    /// Total scanlines per frame (visible + blanking).
    constexpr std::uint32_t vTotal() const {
        return static_cast<std::uint32_t>(vActive) + vFrontPorch + vSyncWidth + vBackPorch;
    }

    /// Horizontal scan rate in Hz (rounded down).
    constexpr std::uint32_t lineFrequencyHz() const {
        return pixelClockHz / hTotal();
    }

    /// Vertical refresh rate in millihertz (rounded down).
    constexpr std::uint32_t refreshRateMilliHz() const {
        return static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(pixelClockHz) * 1000u) / (hTotal() * vTotal()));
    }
};

} // namespace picottl
