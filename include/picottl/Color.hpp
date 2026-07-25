// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Color.hpp
// Device-independent color value.
//
// PUBLIC API - FROZEN.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace picottl {

/// Device-independent color: a small abstract index.
///
/// FRAMEWORK INVARIANT (design principle 20): the index is an abstract
/// pixel value, never an electrical one. Rendering code (Graphics,
/// Text) and surfaces treat it as opaque: packed framebuffers store its
/// low bits verbatim, and only the active display backend defines what
/// those bits mean (electrically or otherwise) - Color{4} is red on
/// CGA and may mean anything else on another backend. Cross-backend
/// conventions: index 0 is black/dark and bit 0 lights a monochrome
/// pixel; richer palettes use higher bits. Backend headers may offer
/// named constants for their own mapping as an optional convenience;
/// raw indices are always equally valid.
struct Color {
    std::uint8_t index = 0;

    constexpr bool operator==(const Color& other) const {
        return index == other.index;
    }
    constexpr bool operator!=(const Color& other) const {
        return index != other.index;
    }
};

namespace colors {

inline constexpr Color kBlack{0};
inline constexpr Color kWhite{1};

} // namespace colors

} // namespace picottl
