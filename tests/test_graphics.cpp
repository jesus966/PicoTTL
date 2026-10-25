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
#include <chrono>
#include <initializer_list>

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

PICOTTL_TEST(extremeRectanglesMatchWideClippingReference) {
    const std::int32_t positions[] = {INT32_MIN, -5, 0, 3, 60, INT32_MAX - 1, INT32_MAX};
    const std::int32_t sizes[] = {INT32_MIN, -1, 0, 1, 10, INT32_MAX};
    for (auto x : positions) for (auto y : positions) {
        for (auto w : sizes) for (auto h : sizes) {
            Fixture f;
            f.gfx.invertRect(x, y, w, h);
            for (std::int32_t row = 0; row < kH; ++row) {
                for (std::int32_t col = 0; col < kW; ++col) {
                    const bool inside = w > 0 && h > 0 && col >= x && row >= y &&
                        col < static_cast<std::int64_t>(x) + w &&
                        row < static_cast<std::int64_t>(y) + h;
                    CHECK_EQ(f.fb.pixel(col, row).index, inside ? 1 : 0);
                }
            }
        }
    }
}

PICOTTL_TEST(extremeCopyRowsPreservesOffsetAndUntouchedPixels) {
    const std::int32_t positions[] = {INT32_MIN, -5, -1, 0, 1, 46, INT32_MAX};
    const std::int32_t counts[] = {INT32_MIN, -1, 0, 1, 7, INT32_MAX};
    for (auto src : positions) for (auto dst : positions) for (auto count : counts) {
        Fixture f;
        for (std::uint16_t y = 0; y < kH; ++y) {
            for (std::uint16_t x = 0; x < kW; ++x) {
                f.fb.setPixel(x, y, Color{static_cast<std::uint8_t>((x + y * 3) % 5 == 0)});
            }
        }
        f.gfx.copyRows(src, dst, count);
        for (std::int64_t y = 0; y < kH; ++y) {
            const std::int64_t offset = y - dst;
            const std::int64_t source = static_cast<std::int64_t>(src) + offset;
            const bool copied = offset >= 0 && offset < count && source >= 0 && source < kH;
            const auto expectedRow = copied ? source : y;
            for (std::uint16_t x = 0; x < kW; ++x) {
                CHECK_EQ(f.fb.pixel(x, y).index, (x + expectedRow * 3) % 5 == 0 ? 1 : 0);
            }
        }
    }
}

PICOTTL_TEST(extremeBitmapCoordinatesMatchClippedPixels) {
    const std::uint8_t data[] = {0xB1, 0x4E};
    const picottl::MonochromeBitmap bitmap{data, 8, 2, 1};
    const std::int32_t positions[] = {INT32_MIN, -7, -1, 0, 47, 63, INT32_MAX - 1, INT32_MAX};
    for (auto x : positions) for (auto y : positions) for (bool opaque : {false, true}) {
        Fixture f;
        f.gfx.clear(Color{1});
        if (opaque) f.gfx.drawBitmap(x, y, bitmap, Color{0}, Color{1});
        else f.gfx.drawBitmap(x, y, bitmap, Color{0});
        for (std::int64_t row = 0; row < kH; ++row) {
            for (std::int64_t col = 0; col < kW; ++col) {
                const auto sx = col - x;
                const auto sy = row - y;
                const bool set = sx >= 0 && sx < 8 && sy >= 0 && sy < 2 &&
                    (data[sy] & (0x80u >> sx)) != 0;
                CHECK_EQ(f.fb.pixel(col, row).index, set ? 0 : 1);
            }
        }
    }
    Fixture f;
    f.gfx.drawBitmap(INT32_MAX, INT32_MAX, {nullptr, 0, 0, 0}, Color{1});
    CHECK_EQ(countLitPixels(f.fb), 0);
}

PICOTTL_TEST(clippedLinesPreserveOriginalBresenhamPixelsInEveryOctant) {
    const std::int32_t endpoints[] = {-8, -1, 0, 1, 4, 7, 12};
    for (auto x0 : endpoints) for (auto y0 : endpoints) {
        for (auto x1 : endpoints) for (auto y1 : endpoints) {
            std::uint8_t actual[6] = {}, expected[6] = {};
            picottl::MonochromeFramebuffer fb(actual, sizeof actual, 8, 6);
            picottl::MonochromeFramebuffer reference(expected, sizeof expected, 8, 6);
            picottl::Graphics gfx(&fb), ref(&reference);
            gfx.drawLine(x0, y0, x1, y1, Color{1});
            // Original symmetric Bresenham, deliberately used only with
            // small endpoints so its work and arithmetic are bounded.
            auto x = x0, y = y0;
            const auto dx = x1 > x0 ? x1 - x0 : x0 - x1;
            const auto dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
            auto error = dx + dy;
            while (true) {
                ref.drawPixel(x, y, Color{1});
                if (x == x1 && y == y1) break;
                const auto doubled = error * 2;
                if (doubled >= dy) { error += dy; x += x0 < x1 ? 1 : -1; }
                if (doubled <= dx) { error += dx; y += y0 < y1 ? 1 : -1; }
            }
            CHECK(std::memcmp(actual, expected, sizeof actual) == 0);
        }
    }
}

PICOTTL_TEST(fullRangeDiagonalLinesAreExactAndTerminatePromptly) {
    const auto started = std::chrono::steady_clock::now();
    Fixture f;
    f.gfx.drawLine(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX, Color{1});
    CHECK_EQ(countLitPixels(f.fb), kH);
    for (std::uint16_t y = 0; y < kH; ++y) CHECK_EQ(f.fb.pixel(y, y).index, 1);
    f.gfx.clear();
    f.gfx.drawLine(INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN, Color{1});
    CHECK_EQ(countLitPixels(f.fb), kH);
    f.gfx.clear();
    f.gfx.drawLine(INT32_MIN, -1, INT32_MAX, 1, Color{1});
    CHECK_EQ(countLitPixels(f.fb), kW);
    for (std::uint16_t x = 0; x < kW; ++x) CHECK_EQ(f.fb.pixel(x, 0).index, 1);
    f.gfx.clear();
    f.gfx.drawLine(1, INT32_MAX, -1, INT32_MIN, Color{1});
    CHECK_EQ(countLitPixels(f.fb), kH);
    for (std::uint16_t y = 0; y < kH; ++y) CHECK_EQ(f.fb.pixel(0, y).index, 1);
    f.gfx.clear();
    f.gfx.drawLine(INT32_MIN, INT32_MAX, INT32_MAX, INT32_MIN, Color{1});
    f.gfx.drawLine(INT32_MIN, 0, -1, INT32_MAX, Color{1});
    CHECK_EQ(countLitPixels(f.fb), 0);
    CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(5));
}

PICOTTL_TEST(extremeAxisSpansClipToActualSurface) {
    Fixture f;
    f.gfx.drawHorizontalLine(INT32_MIN, 0, INT32_MAX, Color{1});
    f.gfx.drawVerticalLine(0, INT32_MIN, INT32_MAX, Color{1});
    f.gfx.drawHorizontalLine(-1, 0, INT32_MIN, Color{1});
    f.gfx.drawVerticalLine(0, -1, INT32_MIN, Color{1});
    CHECK_EQ(countLitPixels(f.fb), 0);
    f.gfx.drawLine(INT32_MIN, 5, INT32_MAX, 5, Color{1});
    f.gfx.drawLine(7, INT32_MAX, 7, INT32_MIN, Color{1});
    CHECK_EQ(countLitPixels(f.fb), kW + kH - 1);
}

namespace {

template <unsigned Depth>
void checkExtremeDrawingAtDepth() {
    std::uint8_t storage[picottl::PackedFramebuffer<Depth>::bytesFor(13, 9)] = {};
    picottl::PackedFramebuffer<Depth> fb(storage, sizeof storage, 13, 9);
    picottl::Graphics gfx(&fb);
    const auto mask = static_cast<std::uint8_t>((1u << Depth) - 1u);
    gfx.clear(Color{1});
    gfx.invertRect(-2, 1, INT32_MAX, 3);
    gfx.drawLine(INT32_MIN, 5, INT32_MAX, 5, Color{mask});
    for (std::uint16_t y = 0; y < 9; ++y) {
        for (std::uint16_t x = 0; x < 13; ++x) {
            const auto expected = y == 5 ? mask : (y >= 1 && y < 4 ? (1u ^ mask) : 1u);
            CHECK_EQ(fb.pixel(x, y).index, expected);
        }
    }
}

} // namespace

PICOTTL_TEST(extremeDrawingPreservesMdaCgaEgaFramebufferDepths) {
    checkExtremeDrawingAtDepth<1>();
    checkExtremeDrawingAtDepth<2>();
    checkExtremeDrawingAtDepth<4>();
    checkExtremeDrawingAtDepth<8>();
}

PICOTTL_TEST(extremeDrawingWithEmptyOrUnboundSurfaceIsInert) {
    std::uint8_t storage = 0x5a;
    picottl::MonochromeFramebuffer fb(&storage, 1, 0, 0);
    picottl::Graphics empty(&fb), unbound;
    for (auto* gfx : {&empty, &unbound}) {
        gfx->drawLine(INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX, Color{1});
        gfx->invertRect(INT32_MIN, INT32_MAX, INT32_MAX, INT32_MIN);
        gfx->copyRows(INT32_MIN, INT32_MAX, INT32_MAX);
        gfx->drawBitmap(INT32_MAX, INT32_MAX, {&storage, 1, 1, 1}, Color{1});
    }
    CHECK_EQ(storage, 0x5a);
}
