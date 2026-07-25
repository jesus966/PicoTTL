// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Graphics.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/Graphics.hpp"

#include <cstddef>

#include "picottl/DisplaySurface.hpp"

namespace picottl {

namespace {

// 64-bit on purpose: differences of full-range int32 coordinates do
// not fit 32 bits, and -INT32_MIN would be undefined.
constexpr std::int64_t absValue(std::int64_t v) {
    return v < 0 ? -v : v;
}

} // namespace

std::int32_t Graphics::width() const {
    return surface_ != nullptr ? surface_->width() : 0;
}

std::int32_t Graphics::height() const {
    return surface_ != nullptr ? surface_->height() : 0;
}

void Graphics::clear(Color color) {
    if (surface_ == nullptr) {
        return;
    }
    surface_->fill(color);
}

void Graphics::drawPixel(std::int32_t x, std::int32_t y, Color color) {
    if (surface_ == nullptr || x < 0 || y < 0 || x > UINT16_MAX || y > UINT16_MAX) {
        return; // The surface clips the upper bounds.
    }
    surface_->setPixel(static_cast<std::uint16_t>(x),
                       static_cast<std::uint16_t>(y), color);
}

void Graphics::drawHorizontalLine(std::int32_t x, std::int32_t y,
                                  std::int32_t length, Color color) {
    if (surface_ == nullptr || length <= 0) {
        return;
    }
    if (x < 0) { // Clip the part left of the surface.
        length += x;
        x = 0;
    }
    if (x > UINT16_MAX) {
        return; // Entirely right of any possible surface.
    }
    if (length > 0x10000 - x) {
        length = 0x10000 - x; // Beyond 65535 clips anyway; also keeps
    }                         // x + i from overflowing.
    for (std::int32_t i = 0; i < length; ++i) {
        drawPixel(x + i, y, color);
    }
}

void Graphics::drawVerticalLine(std::int32_t x, std::int32_t y,
                                std::int32_t length, Color color) {
    if (surface_ == nullptr || length <= 0) {
        return;
    }
    if (y < 0) { // Clip the part above the surface.
        length += y;
        y = 0;
    }
    if (y > UINT16_MAX) {
        return; // Entirely below any possible surface.
    }
    if (length > 0x10000 - y) {
        length = 0x10000 - y;
    }
    for (std::int32_t i = 0; i < length; ++i) {
        drawPixel(x, y + i, color);
    }
}

void Graphics::drawLine(std::int32_t x0, std::int32_t y0,
                        std::int32_t x1, std::int32_t y1, Color color) {
    if (surface_ == nullptr) {
        return;
    }
    // Axis-aligned fast paths.
    if (y0 == y1) {
        // Clip in 64 bits: the span of full-range endpoints does not
        // fit int32, and the visible window is at most 65536 pixels.
        std::int64_t xStart = x0 < x1 ? x0 : x1;
        std::int64_t span = absValue(static_cast<std::int64_t>(x1) - x0) + 1;
        if (xStart < 0) {
            span += xStart;
            xStart = 0;
        }
        if (span <= 0 || xStart > UINT16_MAX) {
            return;
        }
        if (span > 0x10000) {
            span = 0x10000;
        }
        drawHorizontalLine(static_cast<std::int32_t>(xStart), y0,
                           static_cast<std::int32_t>(span), color);
        return;
    }
    if (x0 == x1) {
        std::int64_t yStart = y0 < y1 ? y0 : y1;
        std::int64_t span = absValue(static_cast<std::int64_t>(y1) - y0) + 1;
        if (yStart < 0) {
            span += yStart;
            yStart = 0;
        }
        if (span <= 0 || yStart > UINT16_MAX) {
            return;
        }
        if (span > 0x10000) {
            span = 0x10000;
        }
        drawVerticalLine(x0, static_cast<std::int32_t>(yStart),
                         static_cast<std::int32_t>(span), color);
        return;
    }

    // Bresenham's line algorithm. Deltas and the error term use 64-bit
    // arithmetic: with full-range int32 endpoints the subtraction alone
    // overflows 32 bits, and the contract promises safe clipping for
    // any signed coordinates. Cost is proportional to the major-axis
    // span even when the segment lies entirely off-screen.
    const std::int64_t dx = absValue(static_cast<std::int64_t>(x1) - x0);
    const std::int64_t dy = -absValue(static_cast<std::int64_t>(y1) - y0);
    const std::int32_t stepX = x0 < x1 ? 1 : -1;
    const std::int32_t stepY = y0 < y1 ? 1 : -1;
    std::int64_t error = dx + dy;

    while (true) {
        drawPixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const std::int64_t doubled = 2 * error;
        if (doubled >= dy) {
            error += dy;
            x0 += stepX;
        }
        if (doubled <= dx) {
            error += dx;
            y0 += stepY;
        }
    }
}

void Graphics::copyRows(std::int32_t srcY, std::int32_t dstY,
                        std::int32_t rowCount) {
    if (surface_ == nullptr) {
        return;
    }
    // Trim the part above the surface, preserving the src->dst offset.
    if (srcY < 0 || dstY < 0) {
        const std::int32_t trim = -(srcY < dstY ? srcY : dstY);
        srcY += trim;
        dstY += trim;
        rowCount -= trim;
    }
    if (rowCount <= 0 || srcY > UINT16_MAX || dstY > UINT16_MAX) {
        return;
    }
    if (rowCount > UINT16_MAX) {
        rowCount = UINT16_MAX;
    }
    surface_->copyRows(static_cast<std::uint16_t>(srcY),
                       static_cast<std::uint16_t>(dstY),
                       static_cast<std::uint16_t>(rowCount));
}

void Graphics::invertRect(std::int32_t x, std::int32_t y,
                          std::int32_t width, std::int32_t height) {
    if (surface_ == nullptr) {
        return;
    }
    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    const std::int32_t maxWidth = surface_->width();
    const std::int32_t maxHeight = surface_->height();
    for (std::int32_t row = y; row < y + height && row < maxHeight; ++row) {
        for (std::int32_t col = x; col < x + width && col < maxWidth; ++col) {
            const Color current =
                surface_->pixel(static_cast<std::uint16_t>(col),
                                static_cast<std::uint16_t>(row));
            surface_->setPixel(
                static_cast<std::uint16_t>(col),
                static_cast<std::uint16_t>(row),
                Color{static_cast<std::uint8_t>(~current.index)});
        }
    }
}

void Graphics::drawBitmap(std::int32_t x, std::int32_t y,
                          const MonochromeBitmap& bitmap, Color foreground) {
    blitBitmap(x, y, bitmap, foreground, colors::kBlack, /*opaque=*/false);
}

void Graphics::drawBitmap(std::int32_t x, std::int32_t y,
                          const MonochromeBitmap& bitmap, Color foreground,
                          Color background) {
    blitBitmap(x, y, bitmap, foreground, background, /*opaque=*/true);
}

void Graphics::blitBitmap(std::int32_t x, std::int32_t y,
                          const MonochromeBitmap& bitmap, Color foreground,
                          Color background, bool opaque) {
    if (surface_ == nullptr || bitmap.data == nullptr) {
        return;
    }
    const std::uint16_t stride = bitmap.effectiveStrideBytes();
    for (std::uint16_t row = 0; row < bitmap.height; ++row) {
        const std::uint8_t* rowData =
            bitmap.data + static_cast<std::size_t>(row) * stride;
        for (std::uint16_t col = 0; col < bitmap.width; ++col) {
            const bool set =
                (rowData[col >> 3] & (0x80u >> (col & 7u))) != 0;
            if (set) {
                drawPixel(x + col, y + row, foreground);
            } else if (opaque) {
                drawPixel(x + col, y + row, background);
            }
        }
    }
}

} // namespace picottl
