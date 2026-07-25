// PicoTTL EGA CRT Diagnostics - EGA-specific patterns
//
// Color diagnostics for six-wire rgbRGB backends. These patterns
// complement the generic catalog (see diagnostics_common/Patterns.hpp)
// with the sections a 64-color monitor family needs.
//
// Backend-knowledge policy: this is the EGA backend's OWN validation
// application, so - unlike the shared patterns - these may rely on the
// EGA backend's DOCUMENTED public color contract (pixel value = IBM
// EGA 6-bit color value in rgbRGB order, picottl/ega/Colors.hpp). They
// still derive ALL geometry from the DiagnosticContext and never touch
// backend internals.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <new>

#include "picottl/PicoTTL.hpp"
#include "picottl/ega/Colors.hpp"
#include "picottl/fonts/IbmEga8x14.hpp"

#include "DiagnosticPattern.hpp"

namespace picottl::diagnostics {

/// Names for the 16 default-palette entries (presentation only).
constexpr const char* kEgaColorNames[16] = {
    "black",      "blue",          "green",       "cyan",
    "red",        "magenta",       "brown",       "lt gray",
    "dk gray",    "lt blue",       "lt green",    "lt cyan",
    "lt red",     "lt magenta",    "yellow",      "white",
};

/// Contrasting label color over a given EGA background value: sums the
/// per-gun levels (primary = 2, secondary = 1) and switches to black
/// text on bright backgrounds.
constexpr Color egaLabelColorOn(std::uint8_t value) {
    const int brightness = 2 * ((value & 1) + ((value >> 1) & 1) + ((value >> 2) & 1)) +
                           ((value >> 3) & 1) + ((value >> 4) & 1) + ((value >> 5) & 1);
    return brightness >= 5 ? Color{0x00} : Color{0x3F};
}

// ---------------------------------------------------------------------------
// All 64 colors
// ---------------------------------------------------------------------------

/// Validates: the complete 64-color gamut on one screen, hex-labeled -
///   an 8x8 grid in numeric order (row-major).
/// Expected: row 0 = the primary-only combinations; each further row
///   adds secondary combinations; 0x14 (row 2, col 4) is brown; 0x3F
///   bottom-right is bright white. Four distinct levels per gun must
///   be discernible across the grid.
/// Defects: two rows identical = a dead secondary wire; two columns
///   identical = a dead primary wire; order wrong = crossed wiring.
class Ega64ColorsPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "64 colors"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        Text& t = c.text;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        const std::int32_t cellW = w / 8;
        const std::int32_t cellH = h / 8;
        g.clear(Color{0});
        t.setUnderline(false);
        const std::int32_t cellCols =
            w / static_cast<std::int32_t>(t.columns());
        const std::int32_t cellRows =
            h / static_cast<std::int32_t>(t.rows());
        for (std::int32_t row = 0; row < 8; ++row) {
            for (std::int32_t col = 0; col < 8; ++col) {
                const auto value = static_cast<std::uint8_t>(row * 8 + col);
                const std::int32_t x0 = col * cellW + 1;
                const std::int32_t y0 = row * cellH + 1;
                const std::int32_t x1 = (col + 1) * cellW - 1;
                const std::int32_t y1 =
                    (row == 7 ? h : (row + 1) * cellH) - 1;
                for (std::int32_t y = y0; y < y1; ++y) {
                    g.drawHorizontalLine(x0, y, x1 - x0, Color{value});
                }
                t.setColors(egaLabelColorOn(value), Color{value});
                t.setCursor((x0 + 2) / cellCols, (y0 + 2) / cellRows);
                t.printf("%02X", value);
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Bit planes
// ---------------------------------------------------------------------------

/// Validates: each of the six color wires in ISOLATION - seven labeled
///   bands, one per pixel-value bit plus an all-bits band.
/// Expected: primary blue, green, red (2/3 amplitude), then secondary
///   blue, green, red (faint, 1/3 amplitude), then bright white.
/// Defects: a band showing the wrong hue = crossed wiring at the DE-9
///   (the connector carries r,R,G,B,g,b on pins 2,3,4,5,6,7 while the
///   GPIOs count B,G,R,b,g,r upward); an empty band = that wire dead;
///   secondary bands as bright as primaries = swapped pri/sec pair.
class EgaBitPlanesPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "rgbRGB bit planes"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        constexpr std::uint8_t kValues[7] = {0x01, 0x02, 0x04,
                                             0x08, 0x10, 0x20, 0x3F};
        constexpr const char* kLabels[7] = {
            "bit 0  PRIMARY BLUE    (DE-9 pin 5)",
            "bit 1  PRIMARY GREEN   (DE-9 pin 4)",
            "bit 2  PRIMARY RED     (DE-9 pin 3)",
            "bit 3  SECONDARY BLUE  (DE-9 pin 7)",
            "bit 4  SECONDARY GREEN (DE-9 pin 6)",
            "bit 5  SECONDARY RED   (DE-9 pin 2)",
            "all bits  BRIGHT WHITE"};
        const std::int32_t band = h / 7;
        g.clear(Color{0});
        Text& t = c.text;
        t.setUnderline(false);
        const std::int32_t cellRows =
            h / static_cast<std::int32_t>(t.rows());
        for (std::int32_t i = 0; i < 7; ++i) {
            const std::int32_t y0 = i * band;
            const std::int32_t y1 = i == 6 ? h : y0 + band;
            for (std::int32_t y = y0; y < y1; ++y) {
                g.drawHorizontalLine(0, y, w, Color{kValues[i]});
            }
            t.setColors(egaLabelColorOn(kValues[i]), Color{kValues[i]});
            t.setCursor(2, (y0 + band / 2) / cellRows);
            t.print(kLabels[i]);
        }
    }
};

// ---------------------------------------------------------------------------
// Gun levels
// ---------------------------------------------------------------------------

/// Validates: the four intensity levels of each gun - three columns
///   (red, green, blue), each split into four horizontal steps: off,
///   secondary only (1/3), primary only (2/3), both (full).
/// Expected: three monotonic single-hue ramps with clearly distinct
///   steps - EGA's defining electrical capability.
/// Defects: only two distinct steps = primary/secondary pair swapped
///   or a dead wire; steps out of order = crossed pri/sec wiring;
///   tinted ramps = monitor purity.
class EgaGunLevelsPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Gun levels"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        // Per gun: {secondary bit, primary bit}.
        constexpr std::uint8_t kSecondary[3] = {0x20, 0x10, 0x08}; // r g b
        constexpr std::uint8_t kPrimary[3] = {0x04, 0x02, 0x01};   // R G B
        const std::int32_t colW = w / 3;
        const std::int32_t stepH = h / 4;
        g.clear(Color{0});
        for (std::int32_t gun = 0; gun < 3; ++gun) {
            const std::uint8_t levels[4] = {
                0x00, kSecondary[gun], kPrimary[gun],
                static_cast<std::uint8_t>(kSecondary[gun] | kPrimary[gun])};
            const std::int32_t x0 = gun * colW + 2;
            const std::int32_t x1 = (gun == 2 ? w : (gun + 1) * colW) - 2;
            for (std::int32_t step = 0; step < 4; ++step) {
                const std::int32_t y0 = step * stepH;
                const std::int32_t y1 = step == 3 ? h : y0 + stepH;
                for (std::int32_t y = y0; y < y1; ++y) {
                    g.drawHorizontalLine(x0, y, x1 - x0,
                                         Color{levels[step]});
                }
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Gray ramp
// ---------------------------------------------------------------------------

/// Validates: the four true achromatic levels (r=g=b): 0x00 black,
///   0x38 dark gray (secondaries), 0x07 light gray (primaries), 0x3F
///   bright white - in ascending labeled bands.
/// Expected: a monotonic 4-step neutral ramp.
/// Defects: non-monotonic = pri/sec swap on some gun; tinted grays =
///   a weak wire or monitor gun balance.
class EgaGrayRampPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Gray ramp"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        constexpr std::uint8_t kRamp[4] = {0x00, 0x38, 0x07, 0x3F};
        constexpr const char* kLabels[4] = {"00 black", "38 dark gray",
                                            "07 light gray",
                                            "3F bright white"};
        const std::int32_t band = w / 4;
        for (std::int32_t i = 0; i < 4; ++i) {
            const std::int32_t x0 = i * band;
            const std::int32_t x1 = i == 3 ? w : x0 + band;
            for (std::int32_t x = x0; x < x1; ++x) {
                g.drawVerticalLine(x, 0, h, Color{kRamp[i]});
            }
        }
        Text& t = c.text;
        t.setUnderline(false);
        const std::int32_t cellCols =
            w / static_cast<std::int32_t>(t.columns());
        for (std::int32_t i = 0; i < 4; ++i) {
            t.setColors(egaLabelColorOn(kRamp[i]), Color{kRamp[i]});
            t.setCursor((i * band + 8) / cellCols, 1);
            t.print(kLabels[i]);
        }
    }
};

// ---------------------------------------------------------------------------
// Default palette
// ---------------------------------------------------------------------------

/// Validates: the IBM power-on palette (the colors period software
///   showed by default), self-naming, plus the CGA-comparison row.
/// Expected: 16 correctly-named colors - brown (0x14) among them with
///   no monitor trickery; each line also shows its raw hex value.
/// Defects: name/color mismatch = wiring or palette table; brown
///   showing as dark yellow = secondary green wire dead.
class EgaDefaultPalettePattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Default palette"; }

    void draw(DiagnosticContext& c) override {
        c.graphics.clear(Color{0});
        Text& t = c.text;
        t.setUnderline(false);
        t.setColors(Color{0x3F}, Color{0});
        t.setCursor(2, 1);
        t.print("IBM EGA power-on palette");
        for (int i = 0; i < 16; ++i) {
            const Color color = picottl::ega::kDefaultPalette[i];
            const std::int32_t col = i < 8 ? 2 : 26;
            const std::int32_t row = 3 + (i % 8);
            t.setColors(i == 0 ? Color{0x38} : color, Color{0});
            t.setCursor(col, row);
            t.printf("%2d 0x%02X %s", i, color.index, kEgaColorNames[i]);
        }
        // Background patches with contrasting digits.
        for (int i = 0; i < 16; ++i) {
            const Color bg = picottl::ega::kDefaultPalette[i];
            t.setColors(egaLabelColorOn(bg.index), bg);
            t.setCursor(2 + i * 3, 13);
            t.printf(" %X ", i);
        }
    }
};

// ---------------------------------------------------------------------------
// Color transitions
// ---------------------------------------------------------------------------

/// Validates: edge behavior when all six wires switch at once, at five
///   transition frequencies - bands repeating the 64-color sequence in
///   16, 8, 4, 2 and 1 pixel columns.
/// Expected: clean transitions down to the 1 px band (softening there
///   is analog bandwidth, not a data defect, if the wider bands are
///   crisp).
/// Defects: ghost columns or ringing on WIDE bands = skew/reflections
///   (check direct signal wiring and the 470 ohm sync resistors); hue bleeding rightward = a
///   slow wire.
class EgaTransitionsPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Color transitions"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        constexpr std::int32_t kWidths[5] = {16, 8, 4, 2, 1};
        const std::int32_t band = h / 5;
        for (std::int32_t b = 0; b < 5; ++b) {
            const std::int32_t y0 = b * band;
            const std::int32_t y1 = b == 4 ? h : y0 + band;
            for (std::int32_t x = 0; x < w; ++x) {
                const auto color =
                    static_cast<std::uint8_t>((x / kWidths[b]) % 64);
                for (std::int32_t y = y0; y < y1; ++y) {
                    g.drawPixel(x, y, Color{color});
                }
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Character set
// ---------------------------------------------------------------------------

/// Validates: every glyph of the 8x14 font (all 256 CP437 codes),
///   drawn directly from the glyph data so control codes render too.
/// Expected: the full IBM character set, 32 glyphs per row, light
///   gray on black.
/// Defects: wrong glyph shapes = font data; ragged columns = 8 bpp
///   text blitting.
class EgaCharacterSetPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Character set"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        g.clear(c.palette.black);
        Text& t = c.text;
        t.setUnderline(false);
        t.setColors(c.palette.bright, c.palette.black);
        t.setCursor(2, 1);
        t.print("Character set (CP437, 8x14)");

        const Font& font = picottl::fonts::kIbmEga8x14;
        const std::int32_t originX = 4 * font.glyphWidth;
        const std::int32_t originY = 3 * font.glyphHeight;
        for (int code = 0; code < 256; ++code) {
            const std::int32_t col = code % 32;
            const std::int32_t row = code / 32;
            g.drawBitmap(originX + col * (font.glyphWidth + 2),
                         originY + row * (font.glyphHeight + 4),
                         font.glyph(static_cast<unsigned char>(code)),
                         c.palette.normal, c.palette.black);
        }
    }
};

// ---------------------------------------------------------------------------
// Terminal behavior
// ---------------------------------------------------------------------------

/// Validates: the full Terminal stack on the active mode - colored
///   attributes, blinking text, underline, block/underline cursor
///   (toggling every 3 s) and auto-scroll (one colored log line every
///   500 ms) with per-cell attribute preservation.
/// Expected: steady header, clean 1 Hz blink, smooth scroll where each
///   line keeps its default-palette color, cursor visible everywhere.
/// Defects: colors bleeding on scroll = cell attribute path; torn
///   scroll = copyRows at this depth; vanished cursor = overlay
///   re-render.
class EgaTerminalPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Terminal behavior"; }

    void draw(DiagnosticContext& c) override {
        namespace ega = picottl::ega;
        terminal_ = new (terminalStorage_)
            Terminal(c.text, cells_, kCellCapacity);
        terminal_->clear();
        terminal_->setAttributes({Color{0x3F}, Color{0x01}, false, false});
        terminal_->printf("%-40s", " Terminal behavior");
        terminal_->setAttributes({ega::kLightRed, Color{0}, false, true});
        terminal_->print("\n BLINKING light red\n");
        terminal_->setAttributes({ega::kLightCyan, Color{0}, true, false});
        terminal_->print(" underlined light cyan\n\n");
        terminal_->setCursorStyle(CursorStyle::Block);
        lineNumber_ = 0;
        lastLineMs_ = 0;
        lastStyleMs_ = 0;
        blockCursor_ = true;
    }

    void update(DiagnosticContext& c, std::uint32_t nowMs) override {
        (void)c;
        if (terminal_ == nullptr) {
            return;
        }
        terminal_->update(nowMs);
        if (nowMs - lastLineMs_ >= 500) {
            lastLineMs_ = nowMs;
            const int index = static_cast<int>(1 + lineNumber_ % 15);
            terminal_->setAttributes(
                {picottl::ega::kDefaultPalette[index], Color{0}, false,
                 false});
            terminal_->printf("scrolling log line %lu\n",
                              static_cast<unsigned long>(lineNumber_));
            ++lineNumber_;
        }
        if (nowMs - lastStyleMs_ >= 3000) {
            lastStyleMs_ = nowMs;
            blockCursor_ = !blockCursor_;
            terminal_->setCursorStyle(blockCursor_ ? CursorStyle::Block
                                                   : CursorStyle::Underline);
        }
    }

private:
    /// Cell capacity for the largest EGA text grid (80x25 at 8x14).
    static constexpr std::size_t kCellCapacity = Terminal::cellsFor(80, 25);

    TerminalCell cells_[kCellCapacity];
    alignas(Terminal) std::uint8_t terminalStorage_[sizeof(Terminal)];
    Terminal* terminal_ = nullptr;
    std::uint32_t lineNumber_ = 0;
    std::uint32_t lastLineMs_ = 0;
    std::uint32_t lastStyleMs_ = 0;
    bool blockCursor_ = true;
};

// ---------------------------------------------------------------------------
// Color cycle (animation)
// ---------------------------------------------------------------------------

/// Validates: dynamic full-bus updates - a 16-bar staircase whose bar
///   values rotate through all 64 colors, one step every 250 ms
///   (state is a pure function of the timestamp).
/// Expected: bars marching smoothly through the gamut; no flicker, no
///   stray pixels.
/// Defects: stray pixels during rotation = DMA/write-path corruption;
///   irregular stepping = timing instability.
class EgaColorCyclePattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Color cycle"; }

    void draw(DiagnosticContext& c) override {
        lastStep_ = ~0u;
        update(c, 0);
    }

    void update(DiagnosticContext& c, std::uint32_t nowMs) override {
        const std::uint32_t step = nowMs / 250u;
        if (step == lastStep_) {
            return;
        }
        lastStep_ = step;
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        const std::int32_t span = w / 16;
        for (std::int32_t i = 0; i < 16; ++i) {
            const auto color =
                static_cast<std::uint8_t>((i * 4 + step) % 64);
            const std::int32_t x0 = i * span;
            const std::int32_t x1 = i == 15 ? w : x0 + span;
            for (std::int32_t x = x0; x < x1; ++x) {
                g.drawVerticalLine(x, 0, h, Color{color});
            }
        }
    }

private:
    std::uint32_t lastStep_ = ~0u;
};

} // namespace picottl::diagnostics
