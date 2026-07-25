// PicoTTL CGA CRT Diagnostics - CGA-specific patterns
//
// Color diagnostics for RGBI backends. These patterns complement the
// generic catalog (see diagnostics_common/Patterns.hpp) with the color
// section a monochrome tool never needed.
//
// Backend-knowledge policy: this is the CGA backend's OWN validation
// application, so - unlike the shared patterns - these may rely on the
// CGA backend's DOCUMENTED public color contract (pixel value = IBM
// RGBI color number, picottl/cga/Colors.hpp). They still derive ALL
// geometry from the DiagnosticContext and never touch backend
// internals: swap in any future backend with the same verbatim RGBI
// contract and they work unchanged.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <new>

#include "picottl/PicoTTL.hpp"
#include "picottl/cga/Colors.hpp"
#include "picottl/fonts/IbmCga8x8.hpp"

#include "DiagnosticPattern.hpp"

namespace picottl::diagnostics {

/// IBM color-number names (presentation only).
constexpr const char* kCgaColorNames[16] = {
    "black",      "blue",          "green",       "cyan",
    "red",        "magenta",       "brown",       "lt gray",
    "dk gray",    "lt blue",       "lt green",    "lt cyan",
    "lt red",     "lt magenta",    "yellow",      "white",
};

/// Contrasting label color over a given RGBI background.
constexpr Color labelColorOn(std::uint8_t background) {
    // Bright backgrounds (8..15) and light gray get black text.
    return (background >= 7) ? Color{0} : Color{15};
}

// ---------------------------------------------------------------------------
// Solid colors
// ---------------------------------------------------------------------------

/// Validates: full-field purity of each of the 16 RGBI colors, and the
///   monitor's HV regulation under uniform load (auto-cycles, 2 s per
///   color, in IBM color-number order; state is a pure function of the
///   timestamp).
/// Expected: uniform fields; color 6 = brown on a genuine 5153,
///   dark yellow on clones - both correct.
/// Defects: hue mismatch vs the printed name = swapped color wires;
///   colors 8-15 identical to 0-7 = INTENSITY dead; breathing edges =
///   monitor HV regulation.
class CgaSolidColorsPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Solid colors 16"; }

    void draw(DiagnosticContext& c) override {
        lastIndex_ = -1; // force repaint on next update
        update(c, 0);
    }

    void update(DiagnosticContext& c, std::uint32_t nowMs) override {
        const auto index = static_cast<std::int32_t>((nowMs / 2000u) % 16u);
        if (index == lastIndex_) {
            return;
        }
        lastIndex_ = index;
        const auto bg = static_cast<std::uint8_t>(index);
        c.graphics.clear(Color{bg});
        c.text.setUnderline(false);
        c.text.setColors(labelColorOn(bg), Color{bg});
        c.text.setCursor(2, 1);
        c.text.printf("color %2ld  %s", static_cast<long>(index),
                      kCgaColorNames[index]);
    }

private:
    std::int32_t lastIndex_ = -1;
};

// ---------------------------------------------------------------------------
// Color bars
// ---------------------------------------------------------------------------

/// Validates: the RGBI staircase (all 16 colors side by side, IBM
///   order) and 4 bpp addressing across bar boundaries.
/// Expected: crisp labeled bars 0..15, dark half then bright half.
/// Defects: order wrong = swapped wires; pairs merged = a color wire
///   stuck; ghost columns at edges = signal skew between wires.
class CgaColorBarsPattern final : public DiagnosticPattern {
public:
    CgaColorBarsPattern(const char* name, bool vertical)
        : name_(name), vertical_(vertical) {}

    const char* name() const override { return name_; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        const std::int32_t span = (vertical_ ? w : h) / 16;
        for (std::int32_t i = 0; i < 16; ++i) {
            const auto color = Color{static_cast<std::uint8_t>(i)};
            const std::int32_t p0 = i * span;
            const std::int32_t p1 = i == 15 ? (vertical_ ? w : h) : p0 + span;
            for (std::int32_t p = p0; p < p1; ++p) {
                if (vertical_) {
                    g.drawVerticalLine(p, 0, h, color);
                } else {
                    g.drawHorizontalLine(0, p, w, color);
                }
            }
        }
        // Hex labels, one per bar, in a contrasting color.
        Text& t = c.text;
        t.setUnderline(false);
        for (std::int32_t i = 0; i < 16; ++i) {
            const auto bg = static_cast<std::uint8_t>(i);
            t.setColors(labelColorOn(bg), Color{bg});
            if (vertical_) {
                const std::int32_t cellW =
                    w / static_cast<std::int32_t>(t.columns());
                t.setCursor((i * span + span / 2) / cellW, 1);
            } else {
                const std::int32_t cellH =
                    h / static_cast<std::int32_t>(t.rows());
                t.setCursor(1, (i * span + span / 2) / cellH);
            }
            t.printf("%lX", static_cast<unsigned long>(i));
        }
    }

private:
    const char* name_;
    bool vertical_;
};

// ---------------------------------------------------------------------------
// RGBI bit planes
// ---------------------------------------------------------------------------

/// Validates: each color wire in ISOLATION - four labeled bands, one
///   per pixel-value bit (Blue, Green, Red, Intensity), plus an
///   all-bits band.
/// Expected: pure blue, pure green, pure red, then the intensity-only
///   band (dark gray - faint by design), then white.
/// Defects: a band showing the WRONG primary = crossed wiring at the
///   DE-9 (the connector carries R,G,B on pins 3,4,5 while GPIOs count
///   B,G,R upward); an empty band = that wire dead; intensity band
///   invisible = INTENSITY wire or monitor summing fault.
class CgaRgbiBitsPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "RGBI bit planes"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        constexpr std::uint8_t kValues[5] = {1, 2, 4, 8, 15};
        constexpr const char* kLabels[5] = {
            "bit 0  BLUE  (DE-9 pin 5)", "bit 1  GREEN (DE-9 pin 4)",
            "bit 2  RED   (DE-9 pin 3)", "bit 3  INTENSITY (DE-9 pin 6)",
            "all bits     WHITE"};
        const std::int32_t band = h / 5;
        g.clear(Color{0});
        for (std::int32_t i = 0; i < 5; ++i) {
            const std::int32_t y0 = i * band;
            const std::int32_t y1 = i == 4 ? h : y0 + band;
            for (std::int32_t y = y0; y < y1; ++y) {
                g.drawHorizontalLine(0, y, w, Color{kValues[i]});
            }
        }
        Text& t = c.text;
        t.setUnderline(false);
        const std::int32_t cellH = h / static_cast<std::int32_t>(t.rows());
        for (std::int32_t i = 0; i < 5; ++i) {
            t.setColors(labelColorOn(kValues[i]), Color{kValues[i]});
            t.setCursor(2, (i * band + band / 2) / cellH);
            t.print(kLabels[i]);
        }
    }
};

// ---------------------------------------------------------------------------
// Primaries and secondaries
// ---------------------------------------------------------------------------

/// Validates: color purity and convergence on the six saturated hues.
///   Six columns (R, G, B primaries; cyan, magenta, dark yellow
///   secondaries), dark half on top, bright half below.
/// Expected: clean saturated columns; the dark yellow / brown cell is
///   the monitor-identity test (brown = genuine 5153 circuit).
/// Defects: fringing at column edges = convergence; tinted columns =
///   purity (degauss the monitor).
class CgaPrimariesPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Primaries/secondaries"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        constexpr std::uint8_t kDark[6] = {4, 2, 1, 3, 5, 6};   // R G B C M Y
        const std::int32_t colW = w / 6;
        for (std::int32_t i = 0; i < 6; ++i) {
            const std::int32_t x0 = i * colW;
            const std::int32_t x1 = i == 5 ? w : x0 + colW;
            for (std::int32_t x = x0; x < x1; ++x) {
                g.drawVerticalLine(x, 0, h / 2, Color{kDark[i]});
                g.drawVerticalLine(
                    x, h / 2, h - h / 2,
                    Color{static_cast<std::uint8_t>(kDark[i] + 8)});
            }
        }
    }
};

// ---------------------------------------------------------------------------
// Intensity ramp (the CGA grayscale)
// ---------------------------------------------------------------------------

/// Validates: the four achromatic levels RGBI can produce - black,
///   dark gray (intensity only), light gray (R+G+B), white (all) - in
///   ascending bands with labels.
/// Expected: a monotonic 4-step gray ramp with neutral (untinted)
///   grays.
/// Defects: non-monotonic ramp = intensity summing; tinted grays =
///   monitor gun balance / a weak color wire.
class CgaIntensityRampPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Intensity ramp"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        constexpr std::uint8_t kRamp[4] = {0, 8, 7, 15};
        constexpr const char* kLabels[4] = {"0  black", "8  dark gray",
                                            "7  light gray", "15 white"};
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
        const std::int32_t cellW = w / static_cast<std::int32_t>(t.columns());
        for (std::int32_t i = 0; i < 4; ++i) {
            t.setColors(labelColorOn(kRamp[i]), Color{kRamp[i]});
            t.setCursor((i * band + 8) / cellW, 1);
            t.print(kLabels[i]);
        }
    }
};

// ---------------------------------------------------------------------------
// Color transitions
// ---------------------------------------------------------------------------

/// Validates: edge behavior when all four wires switch at once, at
///   five transition frequencies - bands repeating the 16-color
///   sequence in 16, 8, 4, 2 and 1 pixel columns.
/// Expected: clean transitions down to the 1 px band (which may soften
///   into a blend at the monitor's bandwidth limit - that is analog,
///   not a data defect, if the wider bands are crisp).
/// Defects: ghost columns or ringing on WIDE bands = skew/reflections
///   (check direct signal wiring and the 470 ohm sync resistors); colors bleeding rightward =
///   a slow wire.
class CgaTransitionsPattern final : public DiagnosticPattern {
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
                const auto color = static_cast<std::uint8_t>(
                    (x / kWidths[b]) % 16);
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

/// Validates: every glyph of the font (all 256 CP437 codes), drawn
///   directly from the glyph data so control codes render too.
/// Expected: the full IBM character set, 32 glyphs per row, in light
///   gray; hex row labels bright.
/// Defects: wrong glyph shapes = font data; ragged columns = 4 bpp
///   text blitting.
class CgaCharacterSetPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Character set"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        g.clear(c.palette.black);
        Text& t = c.text;
        t.setUnderline(false);
        t.setColors(c.palette.bright, c.palette.black);
        t.setCursor(2, 1);
        t.print("Character set (CP437)");

        const Font& font = picottl::fonts::kIbmCga8x8;
        const std::int32_t originX = 4 * font.glyphWidth;
        const std::int32_t originY = 3 * font.glyphHeight;
        for (int code = 0; code < 256; ++code) {
            const std::int32_t col = code % 32;
            const std::int32_t row = code / 32;
            g.drawBitmap(originX + col * (font.glyphWidth + 1),
                         originY + row * (font.glyphHeight + 4),
                         font.glyph(static_cast<unsigned char>(code)),
                         c.palette.normal, c.palette.black);
        }
    }
};

// ---------------------------------------------------------------------------
// ANSI colors through the terminal stack
// ---------------------------------------------------------------------------

/// Validates: the 16 foreground and 16 background colors through the
///   Text attribute path (the same path AnsiTerminal drives), fg over
///   black and labeled bg patches.
/// Expected: two blocks: 16 self-naming foreground lines in two
///   columns, then a row of 16 background patches with contrasting
///   digits.
/// Defects: fg/bg mismatch = attribute plumbing; unreadable patches =
///   labelColorOn logic vs monitor rendering.
class CgaAnsiColorsPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "ANSI colors"; }

    void draw(DiagnosticContext& c) override {
        c.graphics.clear(Color{0});
        Text& t = c.text;
        t.setUnderline(false);
        t.setColors(Color{15}, Color{0});
        t.setCursor(2, 1);
        t.print("ANSI / SGR colors (identity palette)");

        // Foregrounds: two columns of self-naming lines.
        for (int i = 0; i < 16; ++i) {
            const std::int32_t col = i < 8 ? 2 : 22;
            const std::int32_t row = 3 + (i % 8);
            t.setColors(Color{static_cast<std::uint8_t>(i)}, Color{0});
            t.setCursor(col, row);
            t.printf("%2d %s", i, kCgaColorNames[i]);
        }

        // Backgrounds: 16 patches with contrasting hex digits.
        for (int i = 0; i < 16; ++i) {
            const auto bg = static_cast<std::uint8_t>(i);
            t.setColors(labelColorOn(bg), Color{bg});
            t.setCursor(2 + i * 2, 13);
            t.printf(" %X", i);
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
///   line keeps its color, cursor visible over any colors.
/// Defects: colors bleeding on scroll = cell attribute path; torn
///   scroll = copyRows at this depth; vanished cursor = overlay
///   re-render.
class CgaTerminalPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Terminal behavior"; }

    void draw(DiagnosticContext& c) override {
        // Rebuild the terminal for the current geometry (Terminal
        // snapshots the grid at construction - by design the only
        // layer that does).
        terminal_ = new (terminalStorage_)
            Terminal(c.text, cells_, kCellCapacity);
        terminal_->clear();
        terminal_->setAttributes({Color{15}, Color{1}, false, false});
        terminal_->printf("%-40s", " Terminal behavior");
        terminal_->setAttributes({Color{12}, Color{0}, false, true});
        terminal_->print("\n BLINKING light red\n");
        terminal_->setAttributes({Color{11}, Color{0}, true, false});
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
            const auto fg = static_cast<std::uint8_t>(1 + lineNumber_ % 15);
            terminal_->setAttributes({Color{fg}, Color{0}, false, false});
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
    /// Cell capacity for the largest CGA text grid (80x25 at 8x8).
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

/// Validates: dynamic full-bus updates - the 16-bar staircase with all
///   bar colors rotating one step every 250 ms (state is a pure
///   function of the timestamp).
/// Expected: bars marching smoothly left; no flicker, no stray pixels.
/// Defects: stray pixels during rotation = DMA/write-path corruption;
///   irregular stepping = timing instability.
class CgaColorCyclePattern final : public DiagnosticPattern {
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
                static_cast<std::uint8_t>((i + step) % 16);
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
