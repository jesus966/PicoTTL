// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// ega/Colors.hpp
// Named color constants and palette tables for the EGA backend.
//
// PUBLIC API - OPTIONAL CONVENIENCE (design principle 20). These
// constants are NOT part of the rendering model: they merely document
// the EGA backend's verbatim pixel-value mapping, where the pixel
// value IS the IBM EGA 6-bit color value in IBM's own "internal card
// bit order" rgbRGB (bit 0 = Primary Blue, bit 1 = Primary Green,
// bit 2 = Primary Red, bit 3 = Secondary Blue, bit 4 = Secondary
// Green, bit 5 = Secondary Red). Applications remain free to use raw
// Color indices instead; both spellings are equally valid.
//
// These names are BACKEND-SCOPED on purpose: Color{4} is red on CGA
// and on EGA, but Color{20} is brown ONLY on EGA - generic color names
// in the common API would lie. Opt-in include; not part of the
// umbrella header.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "picottl/Color.hpp"

namespace picottl::ega {

// The sixteen colors of the EGA power-on (default) palette, named by
// their CGA-heritage appearance. The full gamut is 64 values (0x00 to
// 0x3F); these are merely the historically prominent ones.
inline constexpr Color kBlack{0x00};
inline constexpr Color kBlue{0x01};
inline constexpr Color kGreen{0x02};
inline constexpr Color kCyan{0x03};
inline constexpr Color kRed{0x04};
inline constexpr Color kMagenta{0x05};
inline constexpr Color kBrown{0x14};       ///< Secondary Green + Primary Red -
                                           ///< the classic EGA brown, no
                                           ///< monitor trickery needed.
inline constexpr Color kLightGray{0x07};
inline constexpr Color kDarkGray{0x38};
inline constexpr Color kLightBlue{0x39};
inline constexpr Color kLightGreen{0x3A};
inline constexpr Color kLightCyan{0x3B};
inline constexpr Color kLightRed{0x3C};
inline constexpr Color kLightMagenta{0x3D};
inline constexpr Color kYellow{0x3E};
inline constexpr Color kWhite{0x3F};

/// The IBM EGA power-on palette: CGA color index (0..15) to EGA color
/// value, exactly as the hardware initialized its 16 palette
/// registers. Palettes are data, not mechanism: PicoTTL's scanout can
/// place all 64 colors in one frame, so recreating a period look is
/// simply a choice of indices. Use these values in 350-line modes.
inline constexpr Color kDefaultPalette[16] = {
    kBlack,    kBlue,       kGreen,      kCyan,
    kRed,      kMagenta,    kBrown,      kLightGray,
    kDarkGray, kLightBlue,  kLightGreen, kLightCyan,
    kLightRed, kLightMagenta, kYellow,   kWhite,
};

/// CGA color index (0..15) to EGA pixel value for the 200-LINE
/// CGA-compatible modes. In those modes a standard 5154 reinterprets
/// the connector as CGA: intensity is Secondary Green (pixel value
/// bit 4) and the Secondary Red/Blue pins are ignored - so CGA color
/// i maps to (i & 7) | ((i & 8) << 1), NOT to kDefaultPalette. Using
/// Packed4 would place "intensity" on an ignored wire; 200-line
/// content on the EGA backend uses Packed8 with these values.
inline constexpr Color kCga200Palette[16] = {
    Color{0x00}, Color{0x01}, Color{0x02}, Color{0x03},
    Color{0x04}, Color{0x05}, Color{0x06}, Color{0x07},
    Color{0x10}, Color{0x11}, Color{0x12}, Color{0x13},
    Color{0x14}, Color{0x15}, Color{0x16}, Color{0x17},
};

} // namespace picottl::ega
