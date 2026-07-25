// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// DisplaySurface.hpp
// A rectangular pixel destination, decoupling rendering from storage.
//
// PUBLIC API - FROZEN. Graphics and Text draw into a DisplaySurface,
// never into a framebuffer directly. Implementations may be monochrome
// or color framebuffers, double buffers, host-side renderers or virtual
// surfaces - rendering code is unaffected by the choice.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "picottl/Color.hpp"

namespace picottl {

/// Abstract rectangular pixel destination.
///
/// Coordinates are zero-based with the origin at the top-left corner.
/// All operations clip: out-of-range writes are ignored and out-of-range
/// reads return colors::kBlack.
///
/// Performance note: per-pixel virtual dispatch is accepted deliberately;
/// rendering happens at drawing time, never on the video signal path.
class DisplaySurface {
public:
    virtual ~DisplaySurface() = default;

    DisplaySurface(const DisplaySurface&) = delete;
    DisplaySurface& operator=(const DisplaySurface&) = delete;

    /// Width in pixels.
    virtual std::uint16_t width() const = 0;

    /// Height in pixels.
    virtual std::uint16_t height() const = 0;

    /// Writes one pixel. Out-of-range coordinates are ignored.
    virtual void setPixel(std::uint16_t x, std::uint16_t y, Color color) = 0;

    /// Reads one pixel. Out-of-range coordinates return colors::kBlack.
    virtual Color pixel(std::uint16_t x, std::uint16_t y) const = 0;

    /// Fills the whole surface with one color.
    virtual void fill(Color color) = 0;

    /// Copies `rowCount` whole LOGICAL pixel rows from `srcY` to `dstY`
    /// within the surface; the regions may overlap (used for scrolling).
    /// Out-of-range rows are clipped.
    ///
    /// This is a logical-surface operation, not a memory operation: each
    /// implementation realizes it however suits its own representation
    /// (a packed framebuffer uses a single memmove; other surfaces may
    /// use entirely different algorithms). The default implementation
    /// copies pixel by pixel; concrete surfaces should override it with
    /// a faster path (virtual with default body: existing
    /// implementations remain source compatible).
    virtual void copyRows(std::uint16_t srcY, std::uint16_t dstY,
                          std::uint16_t rowCount);

protected:
    DisplaySurface() = default;
};

} // namespace picottl
