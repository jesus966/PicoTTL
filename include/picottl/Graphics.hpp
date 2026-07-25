// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Graphics.hpp
// Drawing primitives over a DisplaySurface.
//
// PUBLIC API - FROZEN (new primitives are added over time; existing
// signatures never change). Platform-independent.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "picottl/Color.hpp"
#include "picottl/MonochromeBitmap.hpp"

namespace picottl {

class DisplaySurface;

/// Drawing primitives over a DisplaySurface.
///
/// Coordinates are signed so that application math may move shapes
/// partially (or fully) off-screen: everything outside the surface is
/// clipped, never an error. When no surface is bound, all operations
/// are no-ops.
///
/// COLOR-AGNOSTIC BY CONTRACT (framework invariant, design principle
/// 20): Graphics only reads and writes Color values, passing them
/// through opaquely. It assumes nothing about color depth, palettes,
/// RGB, brightness or channel ordering - the active backend alone
/// defines what a pixel value means. The only conventions used are the
/// framework-wide ones: the default clear color is index 0 (renders
/// dark on every backend), and invertRect() is a bitwise operation
/// whose visual meaning is backend-defined.
///
/// Obtain one from Display::graphics(), or construct one directly over
/// any DisplaySurface (e.g. an off-screen framebuffer, or a mock in
/// unit tests).
class Graphics {
public:
    /// Binds the given surface (nullptr = unbound; operations no-op).
    explicit Graphics(DisplaySurface* surface = nullptr)
        : surface_(surface) {}

    /// True when a surface is bound.
    bool isBound() const { return surface_ != nullptr; }

    /// Width in pixels of the bound surface (0 when unbound). Always
    /// derived live from the surface: Graphics caches no geometry, so
    /// rebinding the surface is always sufficient.
    std::int32_t width() const;

    /// Height in pixels of the bound surface (0 when unbound). Always
    /// derived live from the surface.
    std::int32_t height() const;

    /// Fills the whole surface with one color.
    void clear(Color color = colors::kBlack);

    /// Draws a single pixel.
    void drawPixel(std::int32_t x, std::int32_t y, Color color);

    /// Draws a horizontal run of `length` pixels starting at (x, y),
    /// extending to the right. Nothing is drawn for length <= 0.
    void drawHorizontalLine(std::int32_t x, std::int32_t y,
                            std::int32_t length, Color color);

    /// Draws a vertical run of `length` pixels starting at (x, y),
    /// extending downwards. Nothing is drawn for length <= 0.
    void drawVerticalLine(std::int32_t x, std::int32_t y,
                          std::int32_t length, Color color);

    /// Draws a straight line between two points (inclusive), using the
    /// axis-aligned fast paths when possible.
    void drawLine(std::int32_t x0, std::int32_t y0,
                  std::int32_t x1, std::int32_t y1, Color color);

    /// Draws a bitmap with (x, y) as its top-left corner. Set bits are
    /// drawn in `foreground`; clear bits are left untouched (transparent).
    void drawBitmap(std::int32_t x, std::int32_t y,
                    const MonochromeBitmap& bitmap, Color foreground);

    /// Draws a bitmap with (x, y) as its top-left corner. Set bits are
    /// drawn in `foreground`, clear bits in `background` (opaque). This
    /// is the rendering path the text renderer builds on.
    void drawBitmap(std::int32_t x, std::int32_t y,
                    const MonochromeBitmap& bitmap, Color foreground,
                    Color background);

    /// Copies `rowCount` whole logical pixel rows from `srcY` to `dstY`
    /// on the bound surface (regions may overlap; out-of-range rows are
    /// clipped). The workhorse of scrolling.
    ///
    /// Note: this is a raster operation on the bound surface rather
    /// than a drawing primitive; it lives in Graphics deliberately, so
    /// higher layers (e.g. Text) keep a single downward dependency.
    void copyRows(std::int32_t srcY, std::int32_t dstY,
                  std::int32_t rowCount);

    /// Inverts a rectangle of pixels (clipped). A GENERAL raster
    /// primitive - useful for selection highlights, debugging flashes,
    /// custom cursors and similar effects; it is not tied to any
    /// specific feature. Inversion is a bitwise NOT of each stored
    /// pixel value (masked to the surface's depth), so the visual
    /// meaning of the result is defined by the active backend.
    void invertRect(std::int32_t x, std::int32_t y,
                    std::int32_t width, std::int32_t height);

private:
    void blitBitmap(std::int32_t x, std::int32_t y,
                    const MonochromeBitmap& bitmap, Color foreground,
                    Color background, bool opaque);

    DisplaySurface* surface_;
};

} // namespace picottl
