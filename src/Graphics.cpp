// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Graphics.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/Graphics.hpp"

#include <cstddef>
#include <algorithm>

#include "picottl/DisplaySurface.hpp"

namespace picottl {

namespace {

// 64-bit on purpose: differences of full-range int32 coordinates do
// not fit 32 bits, and -INT32_MIN would be undefined.
constexpr std::int64_t absValue(std::int64_t v) {
    return v < 0 ? -v : v;
}

// Half-open visible interval. Widen only the endpoint calculation;
// successful bounds fit the surface's uint16 geometry and int32 loops.
bool clipSpan(std::int32_t start, std::int32_t length, std::int32_t limit,
              std::int32_t& first, std::int32_t& end) {
    if (length <= 0) {
        return false;
    }
    first = std::max<std::int32_t>(start, 0);
    end = static_cast<std::int32_t>(std::min<std::int64_t>(
        static_cast<std::int64_t>(start) + length, limit));
    return first < end;
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
    if (surface_ == nullptr || y < 0 || y >= height()) {
        return;
    }
    std::int32_t first, end;
    if (!clipSpan(x, length, width(), first, end)) {
        return;
    }
    for (std::int32_t col = first; col < end; ++col) {
        drawPixel(col, y, color);
    }
}

void Graphics::drawVerticalLine(std::int32_t x, std::int32_t y,
                                std::int32_t length, Color color) {
    if (surface_ == nullptr || x < 0 || x >= width()) {
        return;
    }
    std::int32_t first, end;
    if (!clipSpan(y, length, height(), first, end)) {
        return;
    }
    for (std::int32_t row = first; row < end; ++row) {
        drawPixel(x, row, color);
    }
}

void Graphics::drawLine(std::int32_t x0, std::int32_t y0,
                        std::int32_t x1, std::int32_t y1, Color color) {
    if (surface_ == nullptr || width() == 0 || height() == 0) {
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

    // Skip invisible major-axis steps without moving the endpoints:
    // geometric endpoint clipping would change Bresenham's rounding phase.
    const auto dx = static_cast<std::uint32_t>(
        absValue(static_cast<std::int64_t>(x1) - x0));
    const auto dy = static_cast<std::uint32_t>(
        absValue(static_cast<std::int64_t>(y1) - y0));
    const bool xMajor = dx >= dy;
    const std::uint32_t major = xMajor ? dx : dy;
    const std::uint32_t minor = xMajor ? dy : dx;
    const std::int32_t majorStart = xMajor ? x0 : y0;
    const std::int32_t minorStart = xMajor ? y0 : x0;
    const std::int32_t majorStep = (xMajor ? x0 < x1 : y0 < y1) ? 1 : -1;
    const std::int32_t minorStep = (xMajor ? y0 < y1 : x0 < x1) ? 1 : -1;
    const std::int32_t majorLimit = xMajor ? width() : height();
    const std::int32_t minorLimit = xMajor ? height() : width();
    const std::int64_t first = std::max<std::int64_t>(0,
        majorStep > 0 ? -static_cast<std::int64_t>(majorStart)
                     : static_cast<std::int64_t>(majorStart) - (majorLimit - 1));
    const std::int64_t last = std::min<std::int64_t>(major,
        majorStep > 0 ? static_cast<std::int64_t>(majorLimit - 1) - majorStart
                     : majorStart);
    if (first > last) {
        return;
    }
    std::uint32_t remainder = major / 2;
    std::int32_t minorPos = minorStart;
    if (first != 0) {
        // k*minor + major/2 fits uint64 for every k <= major, including
        // full-range int32 deltas. Divide only when skipping initial pixels.
        const std::uint64_t phase = static_cast<std::uint64_t>(first) * minor + major / 2;
        remainder = static_cast<std::uint32_t>(phase % major);
        minorPos = static_cast<std::int32_t>(
            static_cast<std::int64_t>(minorStart) +
            minorStep * static_cast<std::int64_t>(phase / major));
    }
    std::int32_t majorPos = static_cast<std::int32_t>(majorStart + majorStep * first);
    const auto steps = static_cast<std::int32_t>(last - first);
    for (std::int32_t i = 0; i <= steps; ++i) {
        if (minorPos >= 0 && minorPos < minorLimit) {
            drawPixel(xMajor ? majorPos : minorPos, xMajor ? minorPos : majorPos, color);
        }
        if (i == steps) {
            break;
        }
        majorPos += majorStep;
        // Equivalent to remainder += minor with carry, without uint32
        // overflow. Both coordinates stay between the original endpoints.
        if (remainder >= major - minor) {
            remainder -= major - minor;
            minorPos += minorStep;
        } else {
            remainder += minor;
        }
    }
}

void Graphics::copyRows(std::int32_t srcY, std::int32_t dstY,
                        std::int32_t rowCount) {
    if (surface_ == nullptr || rowCount <= 0) {
        return;
    }
    const std::int32_t h = height();
    const std::int32_t first = std::min(srcY, dstY);
    if (first < 0) {
        // rowCount is positive, so -rowCount is representable. Reject
        // wholly clipped spans before ever negating a possible INT32_MIN.
        if (first <= -rowCount) {
            return;
        }
        const std::int32_t trim = -first;
        // Check before adding trim: the other start could be INT32_MAX.
        if (srcY >= h - trim || dstY >= h - trim) {
            return;
        }
        srcY += trim;
        dstY += trim;
        rowCount -= trim;
    }
    if (srcY >= h || dstY >= h) {
        return;
    }
    rowCount = std::min(rowCount, h - std::max(srcY, dstY));
    surface_->copyRows(static_cast<std::uint16_t>(srcY),
                       static_cast<std::uint16_t>(dstY),
                       static_cast<std::uint16_t>(rowCount));
}

void Graphics::invertRect(std::int32_t x, std::int32_t y,
                          std::int32_t width, std::int32_t height) {
    if (surface_ == nullptr) {
        return;
    }
    std::int32_t left, right, top, bottom;
    if (!clipSpan(x, width, surface_->width(), left, right) ||
        !clipSpan(y, height, surface_->height(), top, bottom)) {
        return;
    }
    for (std::int32_t row = top; row < bottom; ++row) {
        for (std::int32_t col = left; col < right; ++col) {
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
    std::int32_t left, right, top, bottom;
    if (!clipSpan(x, bitmap.width, width(), left, right) ||
        !clipSpan(y, bitmap.height, height(), top, bottom)) {
        return;
    }
    // A visible bitmap (uint16 dimensions) bounds these differences.
    const std::int32_t sourceX = left - x;
    const std::int32_t sourceY = top - y;
    const std::uint16_t stride = bitmap.effectiveStrideBytes();
    for (std::int32_t row = 0; row < bottom - top; ++row) {
        const std::uint8_t* rowData =
            bitmap.data + static_cast<std::size_t>(sourceY + row) * stride;
        for (std::int32_t col = 0; col < right - left; ++col) {
            const auto bit = static_cast<std::uint32_t>(sourceX + col);
            const bool set =
                (rowData[bit >> 3] & (0x80u >> (bit & 7u))) != 0;
            if (set) {
                drawPixel(left + col, top + row, foreground);
            } else if (opaque) {
                drawPixel(left + col, top + row, background);
            }
        }
    }
}

} // namespace picottl
