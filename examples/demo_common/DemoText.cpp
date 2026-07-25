// PicoTTL showcase demonstrations - shared text-mode utilities.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "DemoText.hpp"

#include <cstring>

#include "pico/stdlib.h"

namespace picottl::demo {

namespace {

struct BoxChars {
    char topLeft, top, topRight;
    char side;
    char bottomLeft, bottom, bottomRight;
};

constexpr BoxChars kSingle = {
    static_cast<char>(0xDA), static_cast<char>(0xC4), static_cast<char>(0xBF),
    static_cast<char>(0xB3),
    static_cast<char>(0xC0), static_cast<char>(0xC4), static_cast<char>(0xD9),
};

constexpr BoxChars kDouble = {
    static_cast<char>(0xC9), static_cast<char>(0xCD), static_cast<char>(0xBB),
    static_cast<char>(0xBA),
    static_cast<char>(0xC8), static_cast<char>(0xCD), static_cast<char>(0xBC),
};

} // namespace

void drawBox(Text& text, std::int32_t column, std::int32_t row,
             std::int32_t columns, std::int32_t rows, BoxStyle style) {
    if (columns < 2 || rows < 2) {
        return;
    }
    const BoxChars& c = style == BoxStyle::Double ? kDouble : kSingle;
    text.setCursor(column, row);
    text.putChar(c.topLeft);
    for (std::int32_t i = 0; i < columns - 2; ++i) {
        text.putChar(c.top);
    }
    text.putChar(c.topRight);
    for (std::int32_t r = 1; r < rows - 1; ++r) {
        text.setCursor(column, row + r);
        text.putChar(c.side);
        text.setCursor(column + columns - 1, row + r);
        text.putChar(c.side);
    }
    text.setCursor(column, row + rows - 1);
    text.putChar(c.bottomLeft);
    for (std::int32_t i = 0; i < columns - 2; ++i) {
        text.putChar(c.bottom);
    }
    text.putChar(c.bottomRight);
}

void fillTextRect(Text& text, std::int32_t column, std::int32_t row,
                  std::int32_t columns, std::int32_t rows, char fill) {
    for (std::int32_t r = 0; r < rows; ++r) {
        text.setCursor(column, row + r);
        for (std::int32_t c = 0; c < columns; ++c) {
            text.putChar(fill);
        }
    }
}

void printCentered(Text& text, std::int32_t row, const char* s) {
    const std::int32_t length =
        static_cast<std::int32_t>(std::strlen(s));
    text.setCursor((text.columns() - length) / 2, row);
    text.print(s);
}

void typeText(Text& text, const char* s, std::uint32_t charDelayMs) {
    for (const char* p = s; *p != '\0'; ++p) {
        text.putChar(*p);
        sleep_ms(charDelayMs);
    }
}

void typeCentered(Text& text, std::int32_t row, const char* s,
                  std::uint32_t charDelayMs) {
    const std::int32_t length =
        static_cast<std::int32_t>(std::strlen(s));
    text.setCursor((text.columns() - length) / 2, row);
    typeText(text, s, charDelayMs);
}

std::int32_t bigTextWidth(const Font& font, const char* s,
                          std::int32_t scaleX) {
    return static_cast<std::int32_t>(std::strlen(s)) * font.glyphWidth *
           scaleX;
}

void drawBigText(Graphics& g, const Font& font, const char* s,
                 std::int32_t x, std::int32_t y,
                 std::int32_t scaleX, std::int32_t scaleY, Color color) {
    std::int32_t penX = x;
    for (const char* p = s; *p != '\0'; ++p) {
        const MonochromeBitmap glyph =
            font.glyph(static_cast<unsigned char>(*p));
        if (glyph.data != nullptr) {
            const std::uint16_t stride = glyph.effectiveStrideBytes();
            for (std::int32_t gy = 0; gy < glyph.height; ++gy) {
                const std::uint8_t* rowBits = glyph.data + gy * stride;
                for (std::int32_t gx = 0; gx < glyph.width; ++gx) {
                    if ((rowBits[gx >> 3] >> (7 - (gx & 7))) & 1u) {
                        for (std::int32_t sy = 0; sy < scaleY; ++sy) {
                            g.drawHorizontalLine(penX + gx * scaleX,
                                                 y + gy * scaleY + sy,
                                                 scaleX, color);
                        }
                    }
                }
            }
        }
        penX += font.glyphWidth * scaleX;
    }
}

void drawTechInfoScreen(Text& text, const TechInfo& info,
                        const ScreenColors& colors) {
    text.setColors(colors.value, colors.background);
    text.clear();

    text.setColors(colors.frame, colors.background);
    drawBox(text, 2, 1, text.columns() - 4, text.rows() - 3,
            BoxStyle::Double);

    text.setColors(colors.heading, colors.background);
    printCentered(text, 3, "TECHNICAL INFORMATION");
    text.setColors(colors.frame, colors.background);
    printCentered(text, 4, "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4"
                           "\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4");

    const std::int32_t labelColumn = 12;
    std::int32_t row = 7;
    auto printLine = [&](const char* label) {
        text.setColors(colors.label, colors.background);
        text.setCursor(labelColumn, row);
        text.printf("%-22s", label);
        text.setColors(colors.value, colors.background);
        row += 2;
    };

    printLine("Video standard . . . :");
    text.printf("%s", info.standard);
    printLine("Video mode . . . . . :");
    text.printf("%s", info.videoMode);
    printLine("Resolution . . . . . :");
    text.printf("%ld x %ld pixels", static_cast<long>(info.width),
                static_cast<long>(info.height));
    printLine("Vertical refresh . . :");
    text.printf("%lu.%02lu Hz",
                static_cast<unsigned long>(info.refreshMilliHz / 1000u),
                static_cast<unsigned long>((info.refreshMilliHz % 1000u) / 10u));
    printLine("Framebuffer format . :");
    text.printf("%s", info.pixelFormat);
    printLine("Framebuffer size . . :");
    text.printf("%lu bytes",
                static_cast<unsigned long>(info.framebufferBytes));
    printLine("Character generator  :");
    text.printf("%s  (%ld x %ld cells)", info.font,
                static_cast<long>(info.columns),
                static_cast<long>(info.rows));
    printLine("PicoTTL backend  . . :");
    text.printf("%s", info.backend);

    text.setColors(colors.label, colors.background);
    printCentered(text, text.rows() - 2,
                  "All values reported by the PicoTTL public API");
}

void drawEndingScreen(Text& text, const char* adapterName,
                      const ScreenColors& colors) {
    text.setColors(colors.value, colors.background);
    text.clear();

    const std::int32_t boxColumns = 52;
    const std::int32_t boxRows = 11;
    const std::int32_t boxColumn = (text.columns() - boxColumns) / 2;
    const std::int32_t boxRow = (text.rows() - boxRows) / 2 - 1;

    text.setColors(colors.frame, colors.background);
    drawBox(text, boxColumn, boxRow, boxColumns, boxRows, BoxStyle::Double);

    text.setColors(colors.heading, colors.background);
    printCentered(text, boxRow + 2, "PicoTTL Demonstration Complete");
    text.setColors(colors.value, colors.background);
    printCentered(text, boxRow + 5, adapterName);
    text.setColors(colors.label, colors.background);
    printCentered(text, boxRow + 8, "Thank you for watching");

    printCentered(text, text.rows() - 2,
                  "* END OF DEMONSTRATION *");
}

} // namespace picottl::demo
