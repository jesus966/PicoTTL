// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// cga/Colors.hpp
// Named color constants and historical palettes for the CGA backend.
//
// PUBLIC API - OPTIONAL CONVENIENCE (design principle 20). These
// constants are NOT part of the rendering model: they merely document
// the CGA backend's verbatim pixel-value mapping, where the pixel
// value IS the IBM RGBI color number (bit 0 = Blue, bit 1 = Green,
// bit 2 = Red, bit 3 = Intensity). Applications remain free to use raw
// Color indices instead; both spellings are equally valid.
//
// These names are BACKEND-SCOPED on purpose: Color{4} is red on CGA
// and may mean anything on another backend, so generic color names in
// the common API would lie. Opt-in include; not part of the umbrella
// header.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "picottl/Color.hpp"

namespace picottl::cga {

// The sixteen RGBI colors, by IBM color number.
inline constexpr Color kBlack{0};
inline constexpr Color kBlue{1};
inline constexpr Color kGreen{2};
inline constexpr Color kCyan{3};
inline constexpr Color kRed{4};
inline constexpr Color kMagenta{5};
inline constexpr Color kBrown{6};        ///< Dark yellow on the wire; a genuine
                                         ///< 5153 darkens it to brown internally.
inline constexpr Color kLightGray{7};
inline constexpr Color kDarkGray{8};
inline constexpr Color kLightBlue{9};
inline constexpr Color kLightGreen{10};
inline constexpr Color kLightCyan{11};
inline constexpr Color kLightRed{12};
inline constexpr Color kLightMagenta{13};
inline constexpr Color kYellow{14};
inline constexpr Color kWhite{15};

/// The historical IBM 320x200 palettes, as plain data (palettes are
/// data, not mechanism: PicoTTL's Packed4 scanout can place all 16
/// colors in one frame, so recreating a period look is simply a
/// choice of indices). Entry 0 is the background, programmable to any
/// of the 16 colors on real hardware - black here as the typical
/// default.
inline constexpr Color kPalette0[4] = {kBlack, kGreen, kRed, kBrown};
inline constexpr Color kPalette0High[4] = {kBlack, kLightGreen, kLightRed,
                                           kYellow};
inline constexpr Color kPalette1[4] = {kBlack, kCyan, kMagenta, kLightGray};
inline constexpr Color kPalette1High[4] = {kBlack, kLightCyan, kLightMagenta,
                                           kWhite};

} // namespace picottl::cga
