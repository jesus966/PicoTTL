// PicoTTL showcase demonstrations - shared text-mode utilities.
//
// CP437 box drawing, typewriter output, scaled banner lettering and
// the shared technical-information / closing screens used by the
// official demonstration programs (examples 96-98).
//
// APPLICATION-LEVEL code on the public PicoTTL API. Colors are always
// supplied by the calling demo (backend-scoped, design principle 20);
// these helpers use whatever colors are currently set on the Text.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "picottl/Color.hpp"
#include "picottl/Font.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/Text.hpp"

namespace picottl::demo {

/// CP437 box-drawing line styles.
enum class BoxStyle : std::uint8_t {
    Single, ///< Light lines   (0xDA 0xC4 0xBF / 0xB3 / 0xC0 0xC4 0xD9).
    Double, ///< Double lines  (0xC9 0xCD 0xBB / 0xBA / 0xC8 0xCD 0xBC).
};

// Frequently used CP437 shading characters.
inline constexpr char kShadeLight = static_cast<char>(0xB0);  // Light shade.
inline constexpr char kShadeMedium = static_cast<char>(0xB1); // Medium shade.
inline constexpr char kShadeDark = static_cast<char>(0xB2);   // Dark shade.
inline constexpr char kBlock = static_cast<char>(0xDB);       // Full block.

/// Draws a box outline on the text grid using the current Text colors.
void drawBox(Text& text, std::int32_t column, std::int32_t row,
             std::int32_t columns, std::int32_t rows, BoxStyle style);

/// Fills a cell rectangle with one character in the current colors
/// (fill = ' ' paints a solid panel of the background color).
void fillTextRect(Text& text, std::int32_t column, std::int32_t row,
                  std::int32_t columns, std::int32_t rows, char fill = ' ');

/// Prints a string horizontally centered on the given row.
void printCentered(Text& text, std::int32_t row, const char* s);

/// Types a string character by character (classic typewriter effect),
/// starting at the current cursor, pausing charDelayMs between glyphs.
void typeText(Text& text, const char* s, std::uint32_t charDelayMs);

/// Types a string centered on the given row.
void typeCentered(Text& text, std::int32_t row, const char* s,
                  std::uint32_t charDelayMs);

/// Pixel width of a string rendered by drawBigText() at scaleX.
std::int32_t bigTextWidth(const Font& font, const char* s,
                          std::int32_t scaleX);

/// Draws a string with each font glyph magnified scaleX x scaleY -
/// the classic "banner" lettering built from the machine's own
/// character generator. Transparent: only set glyph pixels are drawn.
void drawBigText(Graphics& g, const Font& font, const char* s,
                 std::int32_t x, std::int32_t y,
                 std::int32_t scaleX, std::int32_t scaleY, Color color);

/// Parameters of the shared technical-information screen. Every field
/// is reported by (or derived from) the public PicoTTL API - the
/// screen exists to show that the demonstrations run on real,
/// queryable hardware capabilities.
struct TechInfo {
    const char* standard = "";       ///< e.g. "IBM Monochrome Display Adapter"
    const char* videoMode = "";      ///< e.g. "IbmMda"
    std::int32_t width = 0;          ///< Display::width()
    std::int32_t height = 0;         ///< Display::height()
    std::uint32_t refreshMilliHz = 0;///< Display::refreshRateMilliHz()
    const char* pixelFormat = "";    ///< e.g. "Packed2 (2 bits per pixel)"
    std::uint32_t framebufferBytes = 0;
    const char* font = "";           ///< e.g. "IBM MDA 9x14"
    std::int32_t columns = 0;        ///< Text::columns()
    std::int32_t rows = 0;           ///< Text::rows()
    const char* backend = "";        ///< e.g. "RP2350 MDA family (PIO + DMA)"
};

/// Colors for the shared framed screens; the demo supplies its
/// backend's palette.
struct ScreenColors {
    Color background;
    Color frame;
    Color heading;
    Color label;
    Color value;
};

/// Draws the classic diagnostics-style technical information screen.
void drawTechInfoScreen(Text& text, const TechInfo& info,
                        const ScreenColors& colors);

/// Draws the shared closing screen: "PicoTTL Demonstration Complete",
/// the adapter name and a thank-you line, inside a double-line frame.
/// The caller then idles forever (the screen is static by design).
void drawEndingScreen(Text& text, const char* adapterName,
                      const ScreenColors& colors);

} // namespace picottl::demo
