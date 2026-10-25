// PicoTTL - host tests: Text
//
// Validates the character renderer: grid geometry derived from surface
// and font, cursor advancement/wrap, glyph rendering equivalence with
// drawBitmap, attributes (colors, underline), printf and scrolling -
// the automated counterparts of hardware Examples 16-18 and 22.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstring>

#include "framework/test.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/PackedFramebuffer.hpp"
#include "picottl/Text.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"

using picottl::Color;

namespace {

constexpr std::uint16_t kW = 720;
constexpr std::uint16_t kH = 350;

struct Fixture {
    std::uint8_t storage[picottl::MonochromeFramebuffer::bytesFor(kW, kH)] = {};
    picottl::MonochromeFramebuffer fb{storage, sizeof storage, kW, kH};
    picottl::Graphics gfx{&fb};
    picottl::Text text{gfx, picottl::fonts::kIbmMda9x14};
};

} // namespace

PICOTTL_TEST(textExtremeCursorPositionsClipBeforeGlyphArithmetic) {
    Fixture f;
    f.text.setUnderline(true);
    f.text.setCursor(INT32_MIN, 0);
    f.text.putChar('A');
    CHECK_EQ(f.text.cursorX(), INT32_MIN + 1);
    f.text.setCursor(0, INT32_MIN);
    f.text.putChar('A');
    CHECK_EQ(f.text.cursorX(), 1);
    f.text.setCursor(0, INT32_MAX);
    f.text.putChar('A');
    CHECK_EQ(f.text.cursorX(), 1);
    CHECK_EQ(f.text.cursorY(), INT32_MAX);
    f.text.print("\n\nA");
    CHECK_EQ(f.text.cursorY(), INT32_MAX);
    f.text.setCursor(INT32_MAX, INT32_MAX);
    f.text.putChar('A'); // right-edge wrap must also saturate the row
    CHECK_EQ(f.text.cursorX(), 1);
    CHECK_EQ(f.text.cursorY(), INT32_MAX);
    for (auto value : f.storage) CHECK_EQ(value, 0);
}

PICOTTL_TEST(textExtremeRightColumnKeepsExistingWrapSemantics) {
    Fixture actual, expected;
    actual.text.setCursor(INT32_MAX, 0);
    actual.text.putChar('A');
    expected.text.setCursor(0, 1);
    expected.text.putChar('A');
    CHECK_EQ(actual.text.cursorX(), 1);
    CHECK_EQ(actual.text.cursorY(), 1);
    CHECK(std::memcmp(actual.storage, expected.storage, sizeof actual.storage) == 0);
}

PICOTTL_TEST(textExtremeFallbackUnderlineAndNegativeAdvance) {
    std::uint8_t storage[8] = {};
    picottl::MonochromeFramebuffer fb(storage, sizeof storage, 8, 8);
    picottl::Graphics gfx(&fb);
    const picottl::Font missing{nullptr, 2, 2, 1, 0, 1};
    picottl::Text text(gfx, missing);
    text.setUnderline(true);
    text.setCursor(INT32_MIN, INT32_MAX);
    text.putChar('A');
    text.setCursor(0, INT32_MAX);
    text.putChar('A');
    for (auto value : storage) CHECK_EQ(value, 0);
    text.setCursor(-1, 0);
    text.print("AA"); // first cell is wholly left; second is visible
    CHECK_EQ(text.cursorX(), 1);
    CHECK_EQ(fb.pixel(0, 1).index, 1);
    CHECK_EQ(fb.pixel(1, 1).index, 1);
    for (std::uint16_t y = 0; y < 8; ++y) {
        for (std::uint16_t x = 0; x < 8; ++x) {
            CHECK_EQ(fb.pixel(x, y).index, y == 1 && x < 2 ? 1 : 0);
        }
    }
}

PICOTTL_TEST(textExtremeScrollClearsOnlyWholeGridRows) {
    std::uint8_t storage[9];
    std::memset(storage, 0xff, sizeof storage);
    picottl::MonochromeFramebuffer fb(storage, sizeof storage, 8, 9);
    picottl::Graphics gfx(&fb);
    const picottl::Font missing{nullptr, 2, 2, 1, 0};
    picottl::Text text(gfx, missing);
    text.scroll(INT32_MIN);
    for (auto value : storage) CHECK_EQ(value, 0xff);
    text.scroll(INT32_MAX);
    for (std::uint16_t y = 0; y < 9; ++y) {
        for (std::uint16_t x = 0; x < 8; ++x) CHECK_EQ(fb.pixel(x, y).index, y == 8 ? 1 : 0);
    }
}

PICOTTL_TEST(textNewlineSaturatesEvenWithoutUsableFont) {
    picottl::Graphics unbound;
    const picottl::Font empty{};
    picottl::Text text(unbound, empty);
    text.setCursor(INT32_MIN, INT32_MAX);
    text.putChar('\n');
    CHECK_EQ(text.cursorX(), 0);
    CHECK_EQ(text.cursorY(), INT32_MAX);
}

PICOTTL_TEST(textGridIsDerivedNotAssumed) {
    Fixture f;
    CHECK_EQ(f.text.columns(), 80); // 720 / 9, derived at runtime
    CHECK_EQ(f.text.rows(), 25);    // 350 / 14

    // A different surface yields a different grid with the same code.
    std::uint8_t small[picottl::MonochromeFramebuffer::bytesFor(90, 42)] = {};
    picottl::MonochromeFramebuffer smallFb(small, sizeof small, 90, 42);
    picottl::Graphics smallGfx(&smallFb);
    picottl::Text smallText(smallGfx, picottl::fonts::kIbmMda9x14);
    CHECK_EQ(smallText.columns(), 10);
    CHECK_EQ(smallText.rows(), 3);
}

PICOTTL_TEST(putCharMatchesDrawBitmapExactly) {
    Fixture f;
    f.text.setCursor(10, 5);
    f.text.putChar('A');

    std::uint8_t reference[sizeof f.storage] = {};
    picottl::MonochromeFramebuffer refFb(reference, sizeof reference, kW, kH);
    picottl::Graphics refGfx(&refFb);
    refGfx.drawBitmap(10 * 9, 5 * 14,
                      picottl::fonts::kIbmMda9x14.glyph('A'),
                      picottl::colors::kWhite, picottl::colors::kBlack);
    CHECK_EQ(std::memcmp(f.storage, reference, sizeof reference), 0);
}

PICOTTL_TEST(cursorAdvancesWrapsAndHandlesControls) {
    Fixture f;
    f.text.setCursor(78, 0);
    f.text.print("xyz"); // wraps after column 79
    CHECK_EQ(f.text.cursorX(), 1);
    CHECK_EQ(f.text.cursorY(), 1);
    f.text.print("\r");
    CHECK_EQ(f.text.cursorX(), 0);
    CHECK_EQ(f.text.cursorY(), 1);
    f.text.print("\n\n");
    CHECK_EQ(f.text.cursorY(), 3);
}

PICOTTL_TEST(underlineDrawsOnFontRow) {
    Fixture f;
    f.text.setUnderline(true);
    f.text.setCursor(0, 0);
    f.text.putChar(' '); // blank glyph + underline
    const std::uint8_t row = picottl::fonts::kIbmMda9x14.underlineRow;
    CHECK_EQ(row, 12); // MDA hardware underline row
    for (std::uint16_t x = 0; x < 9; ++x) {
        CHECK_EQ(f.fb.pixel(x, row).index, 1);
    }
    CHECK_EQ(f.fb.pixel(0, row - 1).index, 0);
}

PICOTTL_TEST(setColorsPaintsCellBackground) {
    std::uint8_t storage[picottl::PackedFramebuffer2::bytesFor(kW, kH)] = {};
    picottl::PackedFramebuffer2 fb(storage, sizeof storage, kW, kH);
    picottl::Graphics gfx(&fb);
    picottl::Text text(gfx, picottl::fonts::kIbmMda9x14);
    text.setColors(Color{3}, Color{1}); // bright on normal
    text.setCursor(0, 0);
    text.putChar(' ');
    for (std::uint16_t x = 0; x < 9; ++x) {
        CHECK_EQ(fb.pixel(x, 0).index, 1); // opaque background painted
    }
}

PICOTTL_TEST(printfFormatsAndTruncatesSafely) {
    Fixture f;
    f.text.setCursor(0, 0);
    f.text.printf("v=%d h=%02X s=%s", 42, 0xAB, "ok");
    CHECK_EQ(f.text.cursorX(), 14); // "v=42 h=AB s=ok"
    // Truncation: far more than kPrintfBufferSize characters.
    f.text.setCursor(0, 5);
    f.text.printf("%0500d", 7);
    // Formatted output is truncated to the buffer, then wrapped by print.
    const std::int32_t written =
        static_cast<std::int32_t>(picottl::Text::kPrintfBufferSize) - 1;
    CHECK_EQ(f.text.cursorY(), 5 + written / 80);
    CHECK_EQ(f.text.cursorX(), written % 80);
}

PICOTTL_TEST(scrollMovesContentAndClearsBottom) {
    Fixture f;
    f.text.setCursor(0, 1);
    f.text.putChar('B');
    std::uint8_t rowBBefore[90 * 14];
    std::memcpy(rowBBefore, f.storage + 1 * 14 * 90, sizeof rowBBefore);

    f.text.scroll(1);
    // Row 1 content moved to row 0.
    CHECK_EQ(std::memcmp(f.storage, rowBBefore, sizeof rowBBefore), 0);
    // Bottom text row cleared.
    for (std::size_t i = 24u * 14u * 90u; i < 25u * 14u * 90u; ++i) {
        CHECK_EQ(f.storage[i], 0x00);
    }
}
