// PicoTTL - host tests: code pages
//
// Validates the code page architecture: Font codePage metadata, the
// widened Font::glyph(uint16_t) accessor, FontFamily metadata-keyed
// lookup, Text family/single-font/escape-hatch semantics, the blank
// fallback glyph, and Terminal::setCodePage whole-screen
// reinterpretation (DOS MODE CON CP SELECT semantics) - the automated
// counterparts of hardware Example 44.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstring>

#include "framework/test.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/PackedFramebuffer.hpp"
#include "picottl/Terminal.hpp"
#include "picottl/Text.hpp"
#include "picottl/fonts/IbmMda9x14Family.hpp"

using picottl::CodePage;
using picottl::Color;
using picottl::Font;
using picottl::FontFamily;

namespace {

constexpr std::uint16_t kW = 720;
constexpr std::uint16_t kH = 350;

struct Fixture {
    std::uint8_t storage[picottl::MonochromeFramebuffer::bytesFor(kW, kH)] = {};
    picottl::MonochromeFramebuffer fb{storage, sizeof storage, kW, kH};
    picottl::Graphics gfx{&fb};
};

// 0xB5 renders a box-drawing fragment under CP437 and A-acute under
// CP850 - a position that visibly distinguishes the datasets.
constexpr unsigned char kDifferingByte = 0xB5;

} // namespace

PICOTTL_TEST(fontsCarryCodePageMetadataAndIdenticalMetrics) {
    const Font& cp437 = picottl::fonts::kIbmMda9x14;
    const Font& cp850 = picottl::fonts::kIbmMda9x14Cp850;
    CHECK(cp437.codePage == CodePage::Cp437);
    CHECK(cp850.codePage == CodePage::Cp850);
    // The family invariant: identical metrics, switchable at runtime.
    CHECK_EQ(cp437.glyphWidth, cp850.glyphWidth);
    CHECK_EQ(cp437.glyphHeight, cp850.glyphHeight);
    CHECK_EQ(cp437.glyphStrideBytes, cp850.glyphStrideBytes);
    CHECK_EQ(cp437.glyphCount, cp850.glyphCount);
    CHECK_EQ(cp437.underlineRow, cp850.underlineRow);
    // The datasets themselves differ (0xB5: frame fragment vs A-acute).
    CHECK(std::memcmp(cp437.glyph(kDifferingByte).data,
                      cp850.glyph(kDifferingByte).data,
                      cp437.bytesPerGlyph()) != 0);
}

PICOTTL_TEST(glyphIndexWideningKeepsByteSemantics) {
    const Font& font = picottl::fonts::kIbmMda9x14;
    CHECK(font.glyph('A').data == font.glyph(std::uint16_t{65}).data);
    // Out-of-range indices yield the empty bitmap.
    CHECK(font.glyph(std::uint16_t{300}).data == nullptr);
}

PICOTTL_TEST(familyLooksUpMembersByTheirOwnMetadata) {
    const FontFamily& family = picottl::fonts::kIbmMda9x14Family;
    CHECK(family.font(CodePage::Cp437) == &picottl::fonts::kIbmMda9x14);
    CHECK(family.font(CodePage::Cp850) == &picottl::fonts::kIbmMda9x14Cp850);

    // A family without a CP850 member reports it as absent.
    const Font* onlyCp437[] = {&picottl::fonts::kIbmMda9x14};
    const FontFamily partial{onlyCp437, 1};
    CHECK(partial.font(CodePage::Cp437) == &picottl::fonts::kIbmMda9x14);
    CHECK(partial.font(CodePage::Cp850) == nullptr);
}

PICOTTL_TEST(textFamilyModeStartsOnCp437AndSwitchesRendering) {
    Fixture f;
    picottl::Text text(f.gfx, picottl::fonts::kIbmMda9x14Family);
    CHECK(text.codePage() == CodePage::Cp437); // Historical default.

    text.setCursor(0, 0);
    text.putChar(static_cast<char>(kDifferingByte));
    std::uint8_t cp437Pixels[sizeof f.storage];
    std::memcpy(cp437Pixels, f.storage, sizeof f.storage);

    CHECK(text.setCodePage(CodePage::Cp850));
    CHECK(text.codePage() == CodePage::Cp850);
    text.setCursor(0, 0);
    text.putChar(static_cast<char>(kDifferingByte));
    CHECK(std::memcmp(cp437Pixels, f.storage, sizeof f.storage) != 0);

    // The CP850 rendering matches a reference draw with that dataset.
    std::uint8_t reference[sizeof f.storage] = {};
    picottl::MonochromeFramebuffer refFb(reference, sizeof reference, kW, kH);
    picottl::Graphics refGfx(&refFb);
    refGfx.drawBitmap(0, 0,
                      picottl::fonts::kIbmMda9x14Cp850.glyph(kDifferingByte),
                      picottl::colors::kWhite, picottl::colors::kBlack);
    CHECK_EQ(std::memcmp(f.storage, reference, sizeof reference), 0);
}

PICOTTL_TEST(singleFontModeOnlyConfirmsItsOwnPage) {
    Fixture f;
    picottl::Text text(f.gfx, picottl::fonts::kIbmMda9x14);
    CHECK(text.setCodePage(CodePage::Cp437)); // Already active.
    CHECK(!text.setCodePage(CodePage::Cp850)); // No family attached.
    CHECK(text.codePage() == CodePage::Cp437);

    picottl::Text cp850Text(f.gfx, picottl::fonts::kIbmMda9x14Cp850);
    CHECK(cp850Text.codePage() == CodePage::Cp850);
    CHECK(cp850Text.setCodePage(CodePage::Cp850));
    CHECK(!cp850Text.setCodePage(CodePage::Cp437));
}

PICOTTL_TEST(setFontDetachesFamilyAndSetFontFamilyRestoresIt) {
    Fixture f;
    picottl::Text text(f.gfx, picottl::fonts::kIbmMda9x14Family);
    CHECK(text.setCodePage(CodePage::Cp850));

    text.setFont(picottl::fonts::kIbmMda9x14); // Low-level takeover.
    CHECK(text.codePage() == CodePage::Cp437);
    CHECK(!text.setCodePage(CodePage::Cp850)); // Family detached.

    text.setFontFamily(picottl::fonts::kIbmMda9x14Family);
    CHECK(text.setCodePage(CodePage::Cp850)); // Managed mode restored.
}

PICOTTL_TEST(missingGlyphRendersAsBlankOpaqueCell) {
    // A custom font with a single glyph: every other byte falls back.
    static const std::uint8_t glyphData[8] = {0xFF, 0xFF, 0xFF, 0xFF,
                                              0xFF, 0xFF, 0xFF, 0xFF};
    const Font tiny{glyphData, 8, 8, 1, 1};

    std::uint8_t storage[picottl::MonochromeFramebuffer::bytesFor(64, 32)];
    std::memset(storage, 0xFF, sizeof storage); // Pre-corrupt: all lit.
    picottl::MonochromeFramebuffer fb(storage, sizeof storage, 64, 32);
    picottl::Graphics gfx(&fb);
    picottl::Text text(gfx, tiny);

    text.setCursor(0, 0);
    text.putChar('A'); // Index 65 >= glyphCount 1: fallback.
    for (std::int32_t y = 0; y < 8; ++y) {
        for (std::int32_t x = 0; x < 8; ++x) {
            CHECK_EQ(fb.pixel(x, y).index, 0u); // Background painted.
        }
    }
}

PICOTTL_TEST(terminalSetCodePageReinterpretsTheWholeScreen) {
    Fixture f;
    picottl::Text text(f.gfx, picottl::fonts::kIbmMda9x14Family);
    static picottl::TerminalCell cells[picottl::Terminal::cellsFor(80, 25)];
    picottl::Terminal terminal(text, cells, sizeof cells / sizeof cells[0]);

    terminal.putChar(static_cast<char>(kDifferingByte));

    // The cell stores the BYTE; switching the page repaints and the
    // same byte reinterprets under the CP850 dataset.
    CHECK(terminal.setCodePage(CodePage::Cp850));
    std::uint8_t reference[sizeof f.storage] = {};
    picottl::MonochromeFramebuffer refFb(reference, sizeof reference, kW, kH);
    picottl::Graphics refGfx(&refFb);
    refGfx.drawBitmap(0, 0,
                      picottl::fonts::kIbmMda9x14Cp850.glyph(kDifferingByte),
                      picottl::colors::kWhite, picottl::colors::kBlack);
    CHECK_EQ(std::memcmp(f.storage, reference, sizeof reference), 0);

    // Switching back restores the CP437 rendering from the cells.
    CHECK(terminal.setCodePage(CodePage::Cp437));
    std::memset(reference, 0, sizeof reference);
    refGfx.drawBitmap(0, 0,
                      picottl::fonts::kIbmMda9x14.glyph(kDifferingByte),
                      picottl::colors::kWhite, picottl::colors::kBlack);
    CHECK_EQ(std::memcmp(f.storage, reference, sizeof reference), 0);
}
