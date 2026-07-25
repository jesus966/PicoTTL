// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Terminal.hpp
// Classic terminal behaviour over a Text renderer.
//
// PUBLIC API - FROZEN (new members are added over time; existing
// signatures never change). Platform-independent.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>

#include "picottl/Color.hpp"
#include "picottl/Text.hpp"

namespace picottl {

/// Visual cursor styles. New styles (e.g. a vertical bar) are added as
/// additive enumerators.
enum class CursorStyle : std::uint8_t {
    None,      ///< Cursor disabled.
    Underline, ///< A line on the font's underline row. Invisible with
               ///< fonts that define no underlineRow (Block always works).
    Block,     ///< Inverse-video cell.
};

/// Per-cell rendering attributes.
///
/// New attributes are added as fields with default initializers, so
/// every existing aggregate initialization remains valid. Color indices
/// are abstract; the active backend defines their meaning.
struct CellAttributes {
    Color foreground = colors::kWhite;
    Color background = colors::kBlack;
    bool underline = false;
    /// The glyph (including its underline) is periodically NOT drawn
    /// during the blink off-phase; the cell background remains. See
    /// Terminal::setTextBlinkPeriodMs().
    bool blink = false;
};

/// One terminal character cell.
struct TerminalCell {
    char character = ' ';
    CellAttributes attributes{};
};

/// Classic terminal behaviour over a Text renderer: a character cell
/// buffer, write operations with automatic scrolling, per-cell
/// attributes including blinking text, screen-clear operations and a
/// non-destructive overlay cursor.
///
/// ARCHITECTURAL INVARIANT - the framebuffer is a cache: the
/// caller-provided cell buffer is the terminal's single source of
/// truth. Overlays (cursor, blink phases) never destroy content -
/// hiding them re-renders the affected cells - and ANY pixel-level
/// corruption, backend reset or framebuffer replacement is recoverable
/// exclusively through repaint().
///
/// FROZEN SCOPE: Terminal is a terminal state machine and Text a pure
/// renderer. Higher-level functionality (ANSI/escape sequences,
/// keyboard handling, windows, status bars, consoles, shells) is built
/// as additive layers ON TOP of Terminal, never by expanding Terminal
/// or Text.
///
/// Geometry is derived from the Text grid at construction and never
/// assumed: the same class works over any surface size, future
/// sub-surfaces or video modes. Character code 0x20 is assumed to
/// render as a blank cell (true for CP437-family fonts).
///
/// Timing is injected: call update(nowMs) periodically (any convenient
/// rate, e.g. once per main-loop iteration) with a monotonic
/// millisecond timestamp of the application's choosing. By design there
/// are no timers, interrupts, heap allocations or platform/SDK
/// dependencies inside Terminal: host backends, RP2350 or any future
/// platform all drive this same function - deterministic and
/// host-testable.
///
/// Usage:
///   static picottl::TerminalCell cells[picottl::Terminal::cellsFor(80, 25)];
///   picottl::Terminal terminal(text, cells, std::size(cells));
///   terminal.setCursorStyle(picottl::CursorStyle::Block);
///   terminal.print("READY.\n");
///   while (true) { terminal.update(nowMs()); ... }
class Terminal {
public:
    /// Cell count required for a given grid (for sizing static buffers).
    static constexpr std::size_t cellsFor(std::int32_t columns,
                                          std::int32_t rows) {
        return columns > 0 && rows > 0
                   ? static_cast<std::size_t>(columns) *
                         static_cast<std::size_t>(rows)
                   : 0u;
    }

    /// @param text      Renderer and geometry source. Must outlive this.
    /// @param cells     Caller-owned cell storage; must outlive this.
    /// @param cellCount Entries in `cells`; if smaller than
    ///                  cellsFor(text.columns(), text.rows()) the
    ///                  terminal is inert (all operations no-op).
    Terminal(Text& text, TerminalCell* cells, std::size_t cellCount);

    /// True when the constructor accepted the cell storage.
    bool isValid() const { return cells_ != nullptr; }

    /// Grid size in cells (derived from the Text at construction).
    std::int32_t columns() const { return columns_; }
    std::int32_t rows() const { return rows_; }

    /// Attributes applied to subsequently written characters.
    void setAttributes(const CellAttributes& attributes);
    const CellAttributes& attributes() const { return attributes_; }

    /// Moves the insertion point (clamped to the grid).
    void setCursor(std::int32_t column, std::int32_t row);
    std::int32_t cursorX() const { return column_; }
    std::int32_t cursorY() const { return row_; }

    /// Writes one character: '\n' and '\r' as usual, automatic wrap at
    /// the right edge and automatic scrolling past the bottom row.
    void putChar(char c);

    /// Writes a zero-terminated string.
    void print(const char* text);

    /// printf-style formatted output (Text::kPrintfBufferSize applies).
    void printf(const char* format, ...) PICOTTL_PRINTF_FORMAT(2, 3);

    /// Clears the whole screen (cells and pixels) with the current
    /// background and homes the insertion point.
    void clear();

    /// Clears the insertion-point row.
    void clearLine();

    /// Clears from the insertion point to the end of its row.
    void clearToEndOfLine();

    /// Clears from the insertion point to the end of the screen.
    void clearToEndOfScreen();

    /// Regenerates the entire visible grid from the logical cell buffer
    /// (the terminal's source of truth), then re-applies the cursor
    /// overlay. Useful after external drawing over the terminal area,
    /// after switching or restoring framebuffers, and for host-side
    /// snapshot testing. Pixels outside the cell grid (partial-cell
    /// margins) are not touched.
    void repaint();

    /// Switches the active code page (see CodePage) and, on success,
    /// repaints: because cells store BYTES, the whole screen instantly
    /// reinterprets under the new page's font - the authentic DOS
    /// MODE CON CP SELECT semantics. Requires the underlying Text to
    /// have been constructed with a FontFamily providing the page
    /// (identical metrics guaranteed by the family invariant, so the
    /// geometry snapshot stays valid); returns false and changes
    /// nothing otherwise. The default is always CodePage::Cp437.
    bool setCodePage(CodePage page);

    /// Selects the visual cursor style (CursorStyle::None disables it).
    /// The cursor is a non-destructive overlay: hiding it restores the
    /// underlying content from the cell buffer.
    void setCursorStyle(CursorStyle style);
    CursorStyle cursorStyle() const { return cursorStyle_; }

    /// Positions the visual cursor independently of the insertion
    /// point. While setCursorFollowsOutput(true) (the default), write
    /// operations move the cursor back to the insertion point.
    void setCursorPosition(std::int32_t column, std::int32_t row);

    /// Couples (default) or decouples the visual cursor from the
    /// insertion point.
    void setCursorFollowsOutput(bool follows);

    /// Full cursor blink cycle in milliseconds (0 = steady). Default 500.
    void setCursorBlinkPeriodMs(std::uint32_t periodMs);

    /// Full blink cycle for blink-attribute text in milliseconds
    /// (0 = always visible). Default 1000.
    void setTextBlinkPeriodMs(std::uint32_t periodMs);

    /// Advances time-driven state (cursor and text blink phases) to
    /// `nowMs`, a monotonic millisecond timestamp supplied by the
    /// application.
    ///
    /// The resulting state is a pure function of the timestamp, never
    /// of the call rate: calling at 1 Hz, 1 kHz or at irregular
    /// intervals yields the same visual state for the same `nowMs`
    /// (intermediate phase toggles between two distant calls are
    /// skipped, not queued).
    void update(std::uint32_t nowMs);

private:
    TerminalCell& cellAt(std::int32_t column, std::int32_t row);
    void renderCell(std::int32_t column, std::int32_t row, bool cursorOverlay);
    void renderBlinkingCells();
    void clearCellRange(std::int32_t fromColumn, std::int32_t fromRow,
                        std::int32_t toColumn, std::int32_t toRow);
    void advanceRow();
    void scrollUp();
    void hideCursorOverlay();
    void showCursorOverlay();

    Text& text_;
    TerminalCell* cells_ = nullptr;
    std::int32_t columns_ = 0;
    std::int32_t rows_ = 0;

    CellAttributes attributes_{};
    std::int32_t column_ = 0; ///< Insertion point.
    std::int32_t row_ = 0;

    CursorStyle cursorStyle_ = CursorStyle::None;
    std::int32_t cursorColumn_ = 0; ///< Visual cursor.
    std::int32_t cursorRow_ = 0;
    bool cursorFollowsOutput_ = true;
    std::uint32_t cursorBlinkPeriodMs_ = 500;
    std::uint32_t textBlinkPeriodMs_ = 1000;
    bool cursorPhaseOn_ = true;
    bool blinkPhaseOn_ = true;
    bool cursorOverlayDrawn_ = false;
};

} // namespace picottl
