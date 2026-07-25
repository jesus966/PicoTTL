// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// DisplaySurface.cpp
// Default (portable, per-pixel) implementation of copyRows(). Concrete
// surfaces override it with faster paths (e.g. MonochromeFramebuffer
// uses a single memmove per call).
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/DisplaySurface.hpp"

namespace picottl {

void DisplaySurface::copyRows(std::uint16_t srcY, std::uint16_t dstY,
                              std::uint16_t rowCount) {
    if (srcY == dstY || rowCount == 0) {
        return;
    }
    const std::uint16_t h = height();
    const std::uint16_t w = width();
    if (srcY >= h || dstY >= h) {
        return;
    }
    const std::uint16_t maxRows =
        static_cast<std::uint16_t>(h - (srcY > dstY ? srcY : dstY));
    if (rowCount > maxRows) {
        rowCount = maxRows;
    }
    // Iterate in the direction that keeps overlapping copies correct.
    if (dstY < srcY) {
        for (std::uint16_t r = 0; r < rowCount; ++r) {
            for (std::uint16_t x = 0; x < w; ++x) {
                setPixel(x, dstY + r, pixel(x, srcY + r));
            }
        }
    } else {
        for (std::uint16_t r = rowCount; r > 0; --r) {
            for (std::uint16_t x = 0; x < w; ++x) {
                setPixel(x, dstY + r - 1, pixel(x, srcY + r - 1));
            }
        }
    }
}

} // namespace picottl
