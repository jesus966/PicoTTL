// PicoTTL - host tests: Graphics
//
// Validates line primitives (endpoints, lengths, symmetry), clipping,
// bitmap rendering (transparent/opaque), rectangle inversion and the
// row-copy raster operation - the automated counterparts of the
// hardware Examples 03-06, 10-13 and 15.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstring>

#include "framework/test.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/PackedFramebuffer.hpp"

using picottl::Color;

namespace {

constexpr std::uint16_t kW = 64;
constexpr std::uint16_t kH = 48;

struct Fixture {
    std::uint8_t storage[picottl::MonochromeFramebuffer::bytesFor(kW, kH)] = {};
    picottl::MonochromeFramebuffer fb{storage, sizeof storage, kW, kH};
    picottl::Graphics gfx{&fb};
};

int countLitPixels(const picottl::MonochromeFramebuffer& fb) {
    int lit = 0;
    for (std::uint16_t y = 0; y < fb.height(); ++y) {
        for (std::uint16_t x = 0; x < fb.width(); ++x) {
            if (fb.pixel(x, y).index != 0) {
                ++lit;
            }
        }
    }
    return lit;
}

} // namespace

PICOTTL_TEST(extremeCoordinatesClipSafelyAndTerminate) {
    Fixture f;
    // Axis-aligned spans wider than int32 used to overflow in the
    // fast-path length math; they must clip to the visible row/column
    // and terminate promptly (the internal cap is 65536 pixels).
    f.gfx.drawLine(-2'000'000'000, 5, 2'000'000'000, 5, Color{1});
    f.gfx.drawLine(7, -2'000'000'000, 7, 2'000'000'000, Color{1});
    f.gfx.drawHorizontalLine(-10, 9, INT32_MAX, Color{1});
    for (std::uint16_t x = 0; x < kW; ++x) {
        CHECK_EQ(f.fb.pixel(x, 5).index, 1u);
        CHECK_EQ(f.fb.pixel(x, 9).index, 1u);
    }
    for (std::uint16_t y = 0; y < kH; ++y) {
        CHECK_EQ(f.fb.pixel(7, y).index, 1u);
    }
    // Fully off-surface extremes draw nothing.
    Fixture g;
    g.gfx.drawHorizontalLine(1'000'000, 5, 100, Color{1});
    g.gfx.drawVerticalLine(5, 1'000'000, 100, Color{1});
    CHECK_EQ(countLitPixels(g.fb), 0);
}

PICOTTL_TEST(graphicsReportsSurfaceSize) {
    Fixture f;
    CHECK_EQ(f.gfx.width(), kW);
    CHECK_EQ(f.gfx.height(), kH);
    picottl::Graphics unbound;
    CHECK(!unbound.isBound());
    CHECK_EQ(unbound.width(), 0);
    unbound.drawPixel(1, 1, Color{1}); // safe no-op
}

PICOTTL_TEST(horizontalLineEndpointsAndLength) {
    Fixture f;
    f.gfx.drawHorizontalLine(5, 7, 10, Color{1});
    CHECK_EQ(f.fb.pixel(4, 7).index, 0);
    CHECK_EQ(f.fb.pixel(5, 7).index, 1);
    CHECK_EQ(f.fb.pixel(14, 7).index, 1);
    CHECK_EQ(f.fb.pixel(15, 7).index, 0);
    CHECK_EQ(countLitPixels(f.fb), 10);
    f.gfx.drawHorizontalLine(0, 8, 0, Color{1});  // length 0: nothing
    f.gfx.drawHorizontalLine(0, 8, -5, Color{1}); // negative: nothing
    CHECK_EQ(countLitPixels(f.fb), 10);
}

PICOTTL_TEST(verticalLineEndpointsAndLength) {
    Fixture f;
    f.gfx.drawVerticalLine(3, 2, 5, Color{1});
    CHECK_EQ(f.fb.pixel(3, 1).index, 0);
    CHECK_EQ(f.fb.pixel(3, 2).index, 1);
    CHECK_EQ(f.fb.pixel(3, 6).index, 1);
    CHECK_EQ(f.fb.pixel(3, 7).index, 0);
    CHECK_EQ(countLitPixels(f.fb), 5);
}

PICOTTL_TEST(lineClipsOffscreenPartsOnly) {
    Fixture f;
    f.gfx.drawHorizontalLine(-4, 0, 8, Color{1}); // 4 visible
    CHECK_EQ(countLitPixels(f.fb), 4);
    CHECK_EQ(f.fb.pixel(0, 0).index, 1);
    CHECK_EQ(f.fb.pixel(3, 0).index, 1);
    f.gfx.clear();
    f.gfx.drawVerticalLine(0, kH - 2, 10, Color{1}); // 2 visible
    CHECK_EQ(countLitPixels(f.fb), 2);
}

PICOTTL_TEST(shallowLineHasOnePixelPerColumn) {
    Fixture f;
    f.gfx.drawLine(2, 3, 37, 17, Color{1});
    CHECK_EQ(f.fb.pixel(2, 3).index, 1);   // endpoints inclusive
    CHECK_EQ(f.fb.pixel(37, 17).index, 1);
    CHECK_EQ(countLitPixels(f.fb), 36);    // max(dx, dy) + 1
    for (std::uint16_t x = 2; x <= 37; ++x) {
        int litInColumn = 0;
        for (std::uint16_t y = 0; y < kH; ++y) {
            if (f.fb.pixel(x, y).index != 0) {
                ++litInColumn;
            }
        }
        CHECK_EQ(litInColumn, 1); // exactly one pixel per major-axis step
    }
}

PICOTTL_TEST(steepLineHasOnePixelPerRow) {
    Fixture f;
    f.gfx.drawLine(14, 40, 10, 2, Color{1}); // steep, drawn upwards
    CHECK_EQ(f.fb.pixel(14, 40).index, 1);
    CHECK_EQ(f.fb.pixel(10, 2).index, 1);
    CHECK_EQ(countLitPixels(f.fb), 39); // max(dx, dy) + 1
    for (std::uint16_t y = 2; y <= 40; ++y) {
        int litInRow = 0;
        for (std::uint16_t x = 0; x < kW; ++x) {
            if (f.fb.pixel(x, y).index != 0) {
                ++litInRow;
            }
        }
        CHECK_EQ(litInRow, 1);
    }
}

PICOTTL_TEST(drawBitmapTransparentAndOpaque) {
    // 8x2 bitmap: top row 10110001, bottom row 01001110.
    static const std::uint8_t data[] = {0xB1, 0x4E};
    const picottl::MonochromeBitmap bitmap{data, 8, 2, 0};

    Fixture f;
    f.gfx.drawHorizontalLine(0, 0, kW, Color{1}); // prefill under the bitmap
    f.gfx.drawHorizontalLine(0, 1, kW, Color{1});
    f.gfx.drawBitmap(2, 0, bitmap, Color{1});     // transparent: bg survives
    CHECK_EQ(f.fb.pixel(3, 0).index, 1); // clear bit, prefilled -> still lit
    f.gfx.drawBitmap(2, 0, bitmap, Color{1}, Color{0}); // opaque
    CHECK_EQ(f.fb.pixel(2, 0).index, 1);
    CHECK_EQ(f.fb.pixel(3, 0).index, 0); // clear bit now painted background
    CHECK_EQ(f.fb.pixel(4, 0).index, 1);
    CHECK_EQ(f.fb.pixel(3, 1).index, 1);
    CHECK_EQ(f.fb.pixel(1, 0).index, 1); // outside the bitmap: untouched
}

PICOTTL_TEST(drawBitmapClipsAtCorners) {
    static const std::uint8_t data[] = {0xFF, 0xFF};
    const picottl::MonochromeBitmap bitmap{data, 8, 2, 0};
    Fixture f;
    f.gfx.drawBitmap(-4, -1, bitmap, Color{1});
    f.gfx.drawBitmap(kW - 4, kH - 1, bitmap, Color{1});
    CHECK_EQ(countLitPixels(f.fb), 4 + 4); // one partial row each
    CHECK_EQ(f.fb.pixel(0, 0).index, 1);
    CHECK_EQ(f.fb.pixel(kW - 1, kH - 1).index, 1);
}

PICOTTL_TEST(invertRectIsAnInvolution) {
    std::uint8_t storage[picottl::PackedFramebuffer2::bytesFor(16, 8)] = {};
    picottl::PackedFramebuffer2 fb(storage, sizeof storage, 16, 8);
    picottl::Graphics gfx(&fb);
    for (std::uint16_t x = 0; x < 16; ++x) {
        fb.setPixel(x, 2, Color{static_cast<std::uint8_t>(x & 3)});
    }
    gfx.invertRect(0, 0, 16, 8);
    CHECK_EQ(fb.pixel(0, 2).index, 3); // ~0 -> 3 at 2 bpp
    CHECK_EQ(fb.pixel(1, 2).index, 2); // ~1 -> 2
    CHECK_EQ(fb.pixel(0, 0).index, 3); // background 0 -> 3
    gfx.invertRect(0, 0, 16, 8);       // inverting twice restores
    CHECK_EQ(fb.pixel(0, 0).index, 0);
    CHECK_EQ(fb.pixel(1, 2).index, 1);
}

PICOTTL_TEST(graphicsCopyRowsTrimsNegativeCoordinates) {
    Fixture f;
    f.gfx.drawHorizontalLine(0, 0, kW, Color{1});
    // srcY -2 with offset +2: the trim must preserve the src->dst offset,
    // so row 0 still lands on row 2.
    f.gfx.copyRows(-2, 0, 3);
    CHECK_EQ(f.fb.pixel(0, 2).index, 1);
    CHECK_EQ(f.fb.pixel(0, 1).index, 0);
}
