// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// MonochromeBitmap.hpp
// Immutable 1-bit-per-pixel bitmap description.
//
// PUBLIC API - FROZEN. Platform-independent.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace picottl {

/// Immutable monochrome bitmap: 1 bpp, row-major, MSB = leftmost pixel.
///
/// A plain descriptor over caller-owned, typically constant data (flash
/// resident on the RP2350). Used by Graphics::drawBitmap() and, in the
/// future, by the text renderer for font glyphs.
struct MonochromeBitmap {
    const std::uint8_t* data = nullptr; ///< Pixel bits, row-major, MSB first.
    std::uint16_t width = 0;            ///< Width in pixels.
    std::uint16_t height = 0;           ///< Height in pixels.
    std::uint16_t strideBytes = 0;      ///< Bytes per row; 0 = packed rows
                                        ///< ((width + 7) / 8), the common case.

    /// Bytes per row after resolving the packed-rows default.
    constexpr std::uint16_t effectiveStrideBytes() const {
        return strideBytes != 0
                   ? strideBytes
                   : static_cast<std::uint16_t>((width + 7u) / 8u);
    }
};

} // namespace picottl
