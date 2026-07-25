// PicoTTL showcase demonstrations - shared drawing utilities.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "DemoDraw.hpp"

#include "pico/stdlib.h"

namespace picottl::demo {

namespace {

/// Half-width of the ellipse row `dy` rows from the center.
std::int32_t ellipseSpan(std::int32_t rx, std::int32_t ry, std::int32_t dy) {
    if (ry <= 0) {
        return rx;
    }
    const std::int64_t ry2 = static_cast<std::int64_t>(ry) * ry;
    const std::int64_t rest = ry2 - static_cast<std::int64_t>(dy) * dy;
    if (rest <= 0) {
        return 0;
    }
    const std::int64_t num = static_cast<std::int64_t>(rx) * rx * rest;
    return static_cast<std::int32_t>(
        isqrt64(static_cast<std::uint64_t>(num / ry2)));
}

} // namespace

void fillRect(Graphics& g, std::int32_t x, std::int32_t y,
              std::int32_t width, std::int32_t height, Color color) {
    for (std::int32_t row = 0; row < height; ++row) {
        g.drawHorizontalLine(x, y + row, width, color);
    }
}

void drawRect(Graphics& g, std::int32_t x, std::int32_t y,
              std::int32_t width, std::int32_t height, Color color) {
    if (width <= 0 || height <= 0) {
        return;
    }
    g.drawHorizontalLine(x, y, width, color);
    g.drawHorizontalLine(x, y + height - 1, width, color);
    g.drawVerticalLine(x, y, height, color);
    g.drawVerticalLine(x + width - 1, y, height, color);
}

void drawEllipse(Graphics& g, std::int32_t cx, std::int32_t cy,
                 std::int32_t rx, std::int32_t ry, Color color) {
    // Polyline over 128 integer-sine segments: smooth at demo sizes,
    // trivially correct, and cheap.
    std::int32_t prevX = cx + rx; // angle 0
    std::int32_t prevY = cy;
    for (std::uint32_t a = 2; a <= 256; a += 2) {
        const std::int32_t px = cx + (rx * icos(a)) / 128;
        const std::int32_t py = cy + (ry * isin(a)) / 128;
        g.drawLine(prevX, prevY, px, py, color);
        prevX = px;
        prevY = py;
    }
}

void fillEllipse(Graphics& g, std::int32_t cx, std::int32_t cy,
                 std::int32_t rx, std::int32_t ry, Color color) {
    for (std::int32_t dy = -ry; dy <= ry; ++dy) {
        const std::int32_t span = ellipseSpan(rx, ry, dy);
        g.drawHorizontalLine(cx - span, cy + dy, 2 * span + 1, color);
    }
}

std::uint8_t bayer64(std::int32_t x, std::int32_t y) {
    // Recursive 8x8 Bayer matrix, computed from the classic 2x2 kernel
    // by bit interleaving (identical values to the tabulated matrix).
    const std::uint32_t ux = static_cast<std::uint32_t>(x) & 7u;
    const std::uint32_t uy = static_cast<std::uint32_t>(y) & 7u;
    const std::uint32_t v = ux ^ uy;
    // Interleave bits of (v, uy); the LEAST significant coordinate bits
    // (finest spatial frequency) drive the MOST significant threshold
    // bits - the defining property of the Bayer ordering.
    std::uint32_t threshold = 0;
    for (int bit = 0; bit < 3; ++bit) {
        threshold = (threshold << 2) | (((v >> bit) & 1u) << 1) |
                    ((uy >> bit) & 1u);
    }
    return static_cast<std::uint8_t>(threshold);
}

void ditherRect(Graphics& g, std::int32_t x, std::int32_t y,
                std::int32_t width, std::int32_t height,
                Color a, Color b, std::int32_t level) {
    if (level <= 0) {
        fillRect(g, x, y, width, height, a);
        return;
    }
    if (level >= 64) {
        fillRect(g, x, y, width, height, b);
        return;
    }
    for (std::int32_t row = 0; row < height; ++row) {
        for (std::int32_t col = 0; col < width; ++col) {
            const Color c =
                level > bayer64(x + col, y + row) ? b : a;
            g.drawPixel(x + col, y + row, c);
        }
    }
}

namespace {

void ditherGradient(Graphics& g, std::int32_t x, std::int32_t y,
                    std::int32_t width, std::int32_t height,
                    const Color* ramp, std::int32_t count, bool vertical) {
    if (count <= 0) {
        return;
    }
    if (count == 1) {
        fillRect(g, x, y, width, height, ramp[0]);
        return;
    }
    const std::int32_t extent = vertical ? height : width;
    if (extent <= 0) {
        return;
    }
    // Position 0..extent-1 maps to ramp progress 0..(count-1)*64.
    const std::int32_t maxProgress = (count - 1) * 64;
    for (std::int32_t major = 0; major < extent; ++major) {
        const std::int32_t progress =
            extent > 1 ? (major * maxProgress) / (extent - 1) : 0;
        std::int32_t segment = progress / 64;
        std::int32_t level = progress % 64;
        if (segment >= count - 1) {
            segment = count - 2;
            level = 64;
        }
        const Color a = ramp[segment];
        const Color b = ramp[segment + 1];
        const std::int32_t minor = vertical ? width : height;
        for (std::int32_t m = 0; m < minor; ++m) {
            const std::int32_t px = vertical ? x + m : x + major;
            const std::int32_t py = vertical ? y + major : y + m;
            g.drawPixel(px, py, level > bayer64(px, py) ? b : a);
        }
    }
}

} // namespace

void ditherVGradient(Graphics& g, std::int32_t x, std::int32_t y,
                     std::int32_t width, std::int32_t height,
                     const Color* ramp, std::int32_t count) {
    ditherGradient(g, x, y, width, height, ramp, count, true);
}

void ditherHGradient(Graphics& g, std::int32_t x, std::int32_t y,
                     std::int32_t width, std::int32_t height,
                     const Color* ramp, std::int32_t count) {
    ditherGradient(g, x, y, width, height, ramp, count, false);
}

std::int32_t isin(std::uint32_t angle) {
    const std::uint32_t a = angle & 255u;
    const std::int32_t x = static_cast<std::int32_t>(a & 127u);
    const std::int32_t value = (x * (128 - x)) >> 5; // 0..128 parabola
    return (a & 128u) ? -value : value;
}

std::uint32_t isqrt64(std::uint64_t value) {
    std::uint64_t result = 0;
    std::uint64_t bit = std::uint64_t{1} << 62;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return static_cast<std::uint32_t>(result);
}

void wipeDown(Graphics& g, Color color, std::int32_t bandHeight,
              std::uint32_t stepDelayMs) {
    if (bandHeight <= 0) {
        bandHeight = 1;
    }
    const std::int32_t width = g.width();
    const std::int32_t height = g.height();
    for (std::int32_t y = 0; y < height; y += bandHeight) {
        fillRect(g, 0, y, width, bandHeight, color);
        sleep_ms(stepDelayMs);
    }
}

void fizzleFade(Graphics& g, Color color, std::uint32_t durationMs) {
    const std::int32_t width = g.width();
    const std::int32_t height = g.height();
    const std::uint32_t total =
        static_cast<std::uint32_t>(width) * static_cast<std::uint32_t>(height);
    if (total == 0) {
        return;
    }
    // 18-bit maximal-length LFSR, taps 18 and 11, written in the
    // right-shift form: the feedback XORs bits 0 and 7 (= 18-11), the
    // reciprocal-polynomial expression of the same taps. Visits
    // 1..2^18-1 exactly once.
    // Values beyond width*height are skipped; pixel (0, 0) is painted
    // explicitly since an LFSR never emits zero.
    constexpr std::uint32_t kPeriod = (1u << 18) - 1u;
    const std::uint32_t chunks = durationMs / 10u > 0 ? durationMs / 10u : 1u;
    const std::uint32_t perChunk = total / chunks + 1u;
    std::uint32_t lfsr = 1;
    std::uint32_t painted = 0;
    std::uint32_t sinceSleep = 0;
    for (std::uint32_t i = 0; i < kPeriod; ++i) {
        const std::uint32_t bit = ((lfsr >> 0) ^ (lfsr >> 7)) & 1u;
        lfsr = (lfsr >> 1) | (bit << 17);
        if (lfsr < total) {
            g.drawPixel(static_cast<std::int32_t>(lfsr % width),
                        static_cast<std::int32_t>(lfsr / width), color);
            ++painted;
            if (++sinceSleep >= perChunk) {
                sinceSleep = 0;
                sleep_ms(10);
            }
        }
        if (painted >= total - 1u) {
            break;
        }
    }
    g.drawPixel(0, 0, color);
}

} // namespace picottl::demo
