// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// PixelFormat.hpp
// Identifiers for the pixel storage formats known to the framework.
//
// PUBLIC API - FROZEN. Platform-independent.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace picottl {

/// Pixel storage formats.
///
/// A format names a memory layout - not a video standard and not a bit
/// count (use bitsPerPixel() for arithmetic). Whether a display backend
/// can scan out a given format is a backend property validated at
/// begin(); backend limitations are never architectural limitations.
/// New formats are added over time; existing values never change.
enum class PixelFormat : std::uint8_t {
    Mono1,   ///< 1-bit pixels, MSB-first.
    Packed2, ///< 2-bit packed pixel fields, MSB-first.
    Packed4, ///< 4-bit packed pixel fields, MSB-first.
    Packed6, ///< 6-bit packed pixel fields, MSB-first (fields may cross byte boundaries).
    Packed8, ///< One byte per pixel.
};

/// Bits per pixel of a format (0 for unknown values).
constexpr unsigned bitsPerPixel(PixelFormat format) {
    switch (format) {
        case PixelFormat::Mono1:
            return 1;
        case PixelFormat::Packed2:
            return 2;
        case PixelFormat::Packed4:
            return 4;
        case PixelFormat::Packed6:
            return 6;
        case PixelFormat::Packed8:
            return 8;
    }
    return 0;
}

} // namespace picottl
