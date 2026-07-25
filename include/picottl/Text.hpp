// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Text.hpp
// Character output over a Graphics context.
//
// PUBLIC API - FROZEN (new members are added over time; existing
// signatures never change). Platform-independent.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "picottl/Color.hpp"
#include "picottl/Font.hpp"

#if defined(__GNUC__)
#define PICOTTL_PRINTF_FORMAT(fmtIndex, argsIndex) \
    __attribute__((format(printf, fmtIndex, argsIndex)))
#else
#define PICOTTL_PRINTF_FORMAT(fmtIndex, argsIndex)
#endif

/// Size in bytes of the stack buffer used by Text::printf(). The default
/// of 128 is intentional: one full 80-column MDA text row plus formatting
/// headroom, rounded up to a power of two - and negligible against the
/// Pico SDK's default 2 KB stack. Override at compile time if an
/// application needs longer single-call output; no heap is ever used.
#ifndef PICOTTL_TEXT_PRINTF_BUFFER
#define PICOTTL_TEXT_PRINTF_BUFFER 128
#endif

namespace picottl {

class Graphics;

/// Character output on a fixed cell grid over a Graphics context.
///
/// FROZEN SCOPE: Text is a pure character renderer. Terminal behaviour
/// (cursor display, blinking, auto-scroll, cell state) lives in
/// picottl::Terminal; anything higher (ANSI, consoles, widgets) is
/// built above Terminal. Text itself does not grow such features.
///
/// The surface is divided into cells of glyphWidth x glyphHeight pixels
/// (80x25 cells with the MDA 9x14 font on a 720x350 mode); the cursor
/// addresses cells: cursorX() is the column, cursorY() the row, origin
/// at the top-left. Glyphs render opaquely (cell background painted)
/// through Graphics::drawBitmap().
///
/// BYTE-TO-GLYPH MODEL: Text renders bytes, not Unicode. Each byte
/// maps to a glyph index (identity for the single-byte IBM code pages:
/// fonts are complete datasets in native byte order, see CodePage) and
/// the index selects a glyph in the active font. A byte whose index
/// falls outside the font renders as the fallback glyph: a blank
/// opaque cell. Text never knows WHICH code page it renders - it only
/// consults the active font.
///
/// Code page selection: construct with a FontFamily (identical-metrics
/// datasets, e.g. fonts::kIbmMda9x14Family) and call setCodePage().
/// The default is always CP437 - historically correct for IBM
/// hardware. setFont() is the low-level escape hatch for custom fonts.
///
/// Scope: byte-oriented output with colors, underline, code page
/// selection, wrap at the right edge, '\n'/'\r' handling, clear() and
/// scroll(). Output below the last row is clipped. Cursor display,
/// blinking and cell state live one layer up, in Terminal.
///
/// Usage:
///   picottl::Text text(display.graphics(), picottl::fonts::kIbmMda9x14);
///   text.setCursor(34, 12);
///   text.printf("Hello %s", "World");
class Text {
public:
    /// printf() formats into a fixed stack buffer of this size (no
    /// allocation); longer output is truncated safely. See
    /// PICOTTL_TEXT_PRINTF_BUFFER for the rationale and the override.
    static constexpr std::size_t kPrintfBufferSize = PICOTTL_TEXT_PRINTF_BUFFER;
    static_assert(kPrintfBufferSize > 0, "printf buffer must not be empty");

    /// @param graphics Drawing context. Must outlive this Text.
    /// @param font     Font resource (typically flash-resident).
    ///                 Must outlive this Text.
    Text(Graphics& graphics, const Font& font);

    /// Constructs in managed (family) mode: the initial font is the
    /// family's CP437 member - the historically correct default - or
    /// its first member if the family has no CP437 dataset. An empty
    /// family yields an inert Text (columns() == 0, output no-ops).
    /// @param graphics Drawing context. Must outlive this Text.
    /// @param family   Font family (identical-metrics datasets, see
    ///                 FontFamily). Must outlive this Text.
    Text(Graphics& graphics, const FontFamily& family);

    /// Selects the font dataset for `page` from the attached family
    /// and returns true; subsequently rendered glyphs use it. Without
    /// a family (single-font construction or after setFont()) it
    /// returns true only when `page` already matches the current
    /// font's metadata, false otherwise; false also when the family
    /// has no dataset for `page` or its metrics differ. Already-drawn
    /// pixels are not touched - Terminal::setCodePage() adds the
    /// authentic whole-screen reinterpretation.
    bool setCodePage(CodePage page);

    /// The code page of the active font (CodePage::Cp437 when inert).
    CodePage codePage() const;

    /// Low-level escape hatch: switches to an explicit font (custom
    /// fonts, datasets outside any family) and DETACHES the attached
    /// family, if any - after this call setCodePage() only confirms
    /// the new font's own page. The font must outlive this Text.
    /// Swapping fonts of different metrics changes the cell grid;
    /// consumers that snapshot geometry (Terminal) must be recreated,
    /// as on a video mode change.
    void setFont(const Font& font);

    /// Attaches (or replaces) the font family and enters managed mode:
    /// selects the member matching the current code page when
    /// available, the family's first member otherwise. An empty family
    /// makes this Text inert. The family must outlive this Text.
    void setFontFamily(const FontFamily& family);

    /// Moves the cursor to a cell position (column, row). Values are not
    /// clamped; drawing at out-of-grid positions is clipped.
    void setCursor(std::int32_t column, std::int32_t row);

    /// Cursor column (cells).
    std::int32_t cursorX() const { return column_; }

    /// Cursor row (cells).
    std::int32_t cursorY() const { return row_; }

    /// Text grid width in cells (surface width / glyph width).
    std::int32_t columns() const;

    /// Text grid height in cells (surface height / glyph height).
    std::int32_t rows() const;

    /// Writes one character. '\n' moves to the start of the next row,
    /// '\r' to the start of the current row; anything else renders a
    /// glyph and advances the cursor, wrapping at the right edge.
    void putChar(char c);

    /// Writes a zero-terminated string.
    void print(const char* text);

    /// printf-style formatted output (see kPrintfBufferSize).
    void printf(const char* format, ...) PICOTTL_PRINTF_FORMAT(2, 3);

    /// Sets the colors used by subsequently rendered glyphs. Defaults:
    /// foreground colors::kWhite, background colors::kBlack. Color
    /// indices are abstract - the active backend defines their meaning
    /// (e.g. on the MDA backend with a Packed2 framebuffer, Color{3} =
    /// VIDEO + INTENSITY = bright text).
    void setColors(Color foreground, Color background);

    /// Current foreground color.
    Color foreground() const { return foreground_; }

    /// Current background color.
    Color background() const { return background_; }

    /// Enables/disables underline for subsequently rendered glyphs.
    /// Drawn on the font's underlineRow in the foreground color; a
    /// no-op for fonts that define no underline position.
    void setUnderline(bool enabled);

    /// True while underline rendering is enabled.
    bool underline() const { return underline_; }

    /// Clears the whole surface to the background color and homes the
    /// cursor to cell (0, 0).
    void clear();

    /// Scrolls the text grid up by `lines` rows: content moves up and
    /// the exposed bottom rows are cleared to the background. The
    /// cursor does not move (callers reposition it, terminal-style).
    /// lines <= 0 does nothing; lines >= rows() clears the grid area.
    /// Only the grid area (rows() x columns() cells) is affected; any
    /// partial-cell band below it is left untouched.
    void scroll(std::int32_t lines = 1);

private:
    void newline();
    void fillPixelRows(std::int32_t y, std::int32_t pixelRows);

    Graphics& graphics_;
    const Font* font_;
    const FontFamily* family_ = nullptr;
    std::int32_t column_ = 0;
    std::int32_t row_ = 0;
    Color foreground_ = colors::kWhite;
    Color background_ = colors::kBlack;
    bool underline_ = false;
};

} // namespace picottl
