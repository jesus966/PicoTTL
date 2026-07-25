// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Terminal.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/Terminal.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace picottl {

Terminal::Terminal(Text& text, TerminalCell* cells, std::size_t cellCount)
    : text_(text) {
    const std::int32_t columns = text.columns();
    const std::int32_t rows = text.rows();
    if (cells == nullptr || cellCount < cellsFor(columns, rows) ||
        columns <= 0 || rows <= 0) {
        return; // Remains inert; all operations are no-ops.
    }
    cells_ = cells;
    columns_ = columns;
    rows_ = rows;
    for (std::size_t i = 0; i < cellsFor(columns, rows); ++i) {
        cells_[i] = TerminalCell{};
    }
}

void Terminal::setAttributes(const CellAttributes& attributes) {
    attributes_ = attributes;
}

void Terminal::setCursor(std::int32_t column, std::int32_t row) {
    if (!isValid()) {
        return;
    }
    hideCursorOverlay();
    column_ = column < 0 ? 0 : (column >= columns_ ? columns_ - 1 : column);
    row_ = row < 0 ? 0 : (row >= rows_ ? rows_ - 1 : row);
    if (cursorFollowsOutput_) {
        cursorColumn_ = column_;
        cursorRow_ = row_;
    }
    showCursorOverlay();
}

void Terminal::putChar(char c) {
    if (!isValid()) {
        return;
    }
    hideCursorOverlay();
    if (c == '\n') {
        column_ = 0;
        advanceRow();
    } else if (c == '\r') {
        column_ = 0;
    } else {
        if (column_ >= columns_) {
            column_ = 0;
            advanceRow(); // Wrap at the right edge.
        }
        TerminalCell& cell = cellAt(column_, row_);
        cell.character = c;
        cell.attributes = attributes_;
        renderCell(column_, row_, false);
        ++column_;
    }
    if (cursorFollowsOutput_) {
        cursorColumn_ = column_ >= columns_ ? columns_ - 1 : column_;
        cursorRow_ = row_;
    }
    showCursorOverlay();
}

void Terminal::print(const char* text) {
    if (text == nullptr) {
        return;
    }
    while (*text != '\0') {
        putChar(*text++);
    }
}

void Terminal::printf(const char* format, ...) {
    char buffer[Text::kPrintfBufferSize];
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    if (written < 0) {
        return; // Encoding error: the buffer contents are unspecified.
    }
    print(buffer);
}

void Terminal::clear() {
    if (!isValid()) {
        return;
    }
    hideCursorOverlay();
    for (std::int32_t i = 0; i < columns_ * rows_; ++i) {
        cells_[i] = TerminalCell{' ', attributes_};
    }
    text_.setColors(attributes_.foreground, attributes_.background);
    text_.setUnderline(false);
    text_.clear();
    column_ = 0;
    row_ = 0;
    if (cursorFollowsOutput_) {
        cursorColumn_ = 0;
        cursorRow_ = 0;
    }
    showCursorOverlay();
}

void Terminal::clearLine() {
    clearCellRange(0, row_, columns_ - 1, row_);
}

void Terminal::clearToEndOfLine() {
    clearCellRange(column_, row_, columns_ - 1, row_);
}

void Terminal::clearToEndOfScreen() {
    clearCellRange(column_, row_, columns_ - 1, rows_ - 1);
}

void Terminal::repaint() {
    if (!isValid()) {
        return;
    }
    // The pixels are regenerated wholesale from the cell buffer, so any
    // previously drawn overlay is gone by definition.
    cursorOverlayDrawn_ = false;
    for (std::int32_t row = 0; row < rows_; ++row) {
        for (std::int32_t column = 0; column < columns_; ++column) {
            renderCell(column, row, false);
        }
    }
    showCursorOverlay();
}

bool Terminal::setCodePage(CodePage page) {
    if (!isValid() || !text_.setCodePage(page)) {
        return false;
    }
    // Cells store bytes: repainting reinterprets the whole screen under
    // the new page's font (DOS MODE CON CP SELECT semantics). The
    // family invariant guarantees unchanged metrics, so the geometry
    // snapshot taken at construction stays valid.
    repaint();
    return true;
}

void Terminal::setCursorStyle(CursorStyle style) {
    if (!isValid()) {
        return;
    }
    hideCursorOverlay();
    cursorStyle_ = style;
    showCursorOverlay();
}

void Terminal::setCursorPosition(std::int32_t column, std::int32_t row) {
    if (!isValid()) {
        return;
    }
    hideCursorOverlay();
    cursorColumn_ = column < 0 ? 0 : (column >= columns_ ? columns_ - 1 : column);
    cursorRow_ = row < 0 ? 0 : (row >= rows_ ? rows_ - 1 : row);
    showCursorOverlay();
}

void Terminal::setCursorFollowsOutput(bool follows) {
    cursorFollowsOutput_ = follows;
}

void Terminal::setCursorBlinkPeriodMs(std::uint32_t periodMs) {
    cursorBlinkPeriodMs_ = periodMs;
}

void Terminal::setTextBlinkPeriodMs(std::uint32_t periodMs) {
    textBlinkPeriodMs_ = periodMs;
}

void Terminal::update(std::uint32_t nowMs) {
    if (!isValid()) {
        return;
    }
    const bool newBlinkPhase =
        textBlinkPeriodMs_ == 0 ||
        (nowMs % textBlinkPeriodMs_) < textBlinkPeriodMs_ / 2;
    if (newBlinkPhase != blinkPhaseOn_) {
        blinkPhaseOn_ = newBlinkPhase;
        hideCursorOverlay();
        renderBlinkingCells();
        showCursorOverlay();
    }
    const bool newCursorPhase =
        cursorBlinkPeriodMs_ == 0 ||
        (nowMs % cursorBlinkPeriodMs_) < cursorBlinkPeriodMs_ / 2;
    if (newCursorPhase != cursorPhaseOn_) {
        cursorPhaseOn_ = newCursorPhase;
        if (cursorPhaseOn_) {
            showCursorOverlay();
        } else {
            hideCursorOverlay();
        }
    }
}

TerminalCell& Terminal::cellAt(std::int32_t column, std::int32_t row) {
    return cells_[row * columns_ + column];
}

void Terminal::renderCell(std::int32_t column, std::int32_t row,
                          bool cursorOverlay) {
    const TerminalCell& cell = cellAt(column, row);
    char character = cell.character;
    Color foreground = cell.attributes.foreground;
    Color background = cell.attributes.background;
    bool underline = cell.attributes.underline;
    if (cell.attributes.blink && !blinkPhaseOn_) {
        // Blink off-phase: the glyph (including its underline) is simply
        // NOT drawn - the cell keeps its background. Modelled as a blank
        // cell rather than as a color trick (fg == bg), so the meaning
        // of the attribute stays independent of renderer policy.
        character = ' ';
        underline = false;
    }
    if (cursorOverlay) {
        if (cursorStyle_ == CursorStyle::Block) {
            // Inverse video from the cell's own colors: correct on every
            // backend (a bitwise pixel inversion would not be - e.g. at
            // 2 bpp it would map "normal" to "intensity only").
            const Color swap = foreground;
            foreground = background;
            background = swap;
        } else if (cursorStyle_ == CursorStyle::Underline) {
            underline = true; // Cursor line on the font's underline row.
        }
    }
    text_.setColors(foreground, background);
    text_.setUnderline(underline);
    text_.setCursor(column, row);
    text_.putChar(character);
}

void Terminal::renderBlinkingCells() {
    for (std::int32_t row = 0; row < rows_; ++row) {
        for (std::int32_t column = 0; column < columns_; ++column) {
            if (cellAt(column, row).attributes.blink) {
                renderCell(column, row, false);
            }
        }
    }
}

void Terminal::clearCellRange(std::int32_t fromColumn, std::int32_t fromRow,
                              std::int32_t toColumn, std::int32_t toRow) {
    if (!isValid()) {
        return;
    }
    hideCursorOverlay();
    std::int32_t column = fromColumn;
    for (std::int32_t row = fromRow; row <= toRow; ++row) {
        for (; column <= (row == toRow ? toColumn : columns_ - 1); ++column) {
            cellAt(column, row) = TerminalCell{' ', attributes_};
            renderCell(column, row, false);
        }
        column = 0;
    }
    showCursorOverlay();
}

void Terminal::advanceRow() {
    if (row_ + 1 < rows_) {
        ++row_;
        return;
    }
    scrollUp();
}

void Terminal::scrollUp() {
    // Text::scroll() clears the exposed pixel rows with the Text
    // background, so align it with the current attributes first.
    text_.setColors(attributes_.foreground, attributes_.background);
    text_.scroll(1);
    std::memmove(cells_, cells_ + columns_,
                 static_cast<std::size_t>(rows_ - 1) * columns_ *
                     sizeof(TerminalCell));
    for (std::int32_t column = 0; column < columns_; ++column) {
        cellAt(column, rows_ - 1) = TerminalCell{' ', attributes_};
    }
    row_ = rows_ - 1;
}

void Terminal::hideCursorOverlay() {
    if (cursorOverlayDrawn_) {
        renderCell(cursorColumn_, cursorRow_, false);
        cursorOverlayDrawn_ = false;
    }
}

void Terminal::showCursorOverlay() {
    if (cursorStyle_ == CursorStyle::None || !cursorPhaseOn_ ||
        cursorOverlayDrawn_) {
        return;
    }
    renderCell(cursorColumn_, cursorRow_, true);
    cursorOverlayDrawn_ = true;
}

} // namespace picottl
