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
#include <limits>

namespace picottl {

/// Electrical polarity of a sync signal.
enum class SyncPolarity : std::uint8_t {
    ActiveHigh, ///< Sync pulse is a logic-high pulse.
    ActiveLow   ///< Sync pulse is a logic-low pulse.
};

/// Complete nominal raster timing description for a TTL video mode.
/// Frequency helpers use pixelClockHz, not a backend's programmed clock or
/// measured output. Hardware divider selection does not modify this object.
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

    /// Nominal horizontal scan rate in Hz (rounded down).
    /// Returns zero if the horizontal total is zero. Zero can also be a
    /// legitimate rounded result, so it is not an unambiguous error indicator.
    constexpr std::uint32_t lineFrequencyHz() const {
        const std::uint32_t total = hTotal();
        if (total == 0) {
            return 0;
        }
        return pixelClockHz / total;
    }

    /// Nominal vertical refresh rate in millihertz (rounded down).
    /// Returns zero if either total is zero or the rounded result exceeds
    /// uint32_t. Zero can also be a legitimate rounded result, so it is not
    /// an unambiguous error indicator. Neither helper validates hardware timing.
    constexpr std::uint32_t refreshRateMilliHz() const {
        const std::uint64_t framePixels = static_cast<std::uint64_t>(hTotal()) * vTotal();
        if (framePixels == 0) {
            return 0;
        }
        const std::uint64_t frequency =
            (static_cast<std::uint64_t>(pixelClockHz) * 1000u) / framePixels;
        if (frequency > std::numeric_limits<std::uint32_t>::max()) {
            return 0;
        }
        return static_cast<std::uint32_t>(frequency);
    }
};

} // namespace picottl
