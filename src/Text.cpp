// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Text.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/Text.hpp"

#include <cstdarg>
#include <cstdio>

#include "picottl/Color.hpp"
#include "picottl/Graphics.hpp"

namespace picottl {

Text::Text(Graphics& graphics, const Font& font)
    : graphics_(graphics), font_(&font) {}

Text::Text(Graphics& graphics, const FontFamily& family)
    : graphics_(graphics), font_(nullptr), family_(&family) {
    font_ = family.font(CodePage::Cp437);
    if (font_ == nullptr && family.memberCount > 0) {
        font_ = family.members[0];
    }
}

bool Text::setCodePage(CodePage page) {
    if (font_ == nullptr) {
        return false;
    }
    if (family_ == nullptr) {
        return page == font_->codePage;
    }
    const Font* candidate = family_->font(page);
    if (candidate == nullptr ||
        candidate->glyphWidth != font_->glyphWidth ||
        candidate->glyphHeight != font_->glyphHeight ||
        candidate->glyphStrideBytes != font_->glyphStrideBytes) {
        return false; // Absent page or family invariant violated.
    }
    font_ = candidate;
    return true;
}

CodePage Text::codePage() const {
    return font_ != nullptr ? font_->codePage : CodePage::Cp437;
}

void Text::setFont(const Font& font) {
    font_ = &font;
    family_ = nullptr; // Explicit low-level takeover.
}

void Text::setFontFamily(const FontFamily& family) {
    const CodePage current = codePage();
    family_ = &family;
    font_ = family.font(current);
    if (font_ == nullptr && family.memberCount > 0) {
        font_ = family.members[0];
    }
}

std::int32_t Text::columns() const {
    return font_ != nullptr && font_->glyphWidth != 0
               ? graphics_.width() / font_->glyphWidth
               : 0;
}

std::int32_t Text::rows() const {
    return font_ != nullptr && font_->glyphHeight != 0
               ? graphics_.height() / font_->glyphHeight
               : 0;
}

void Text::setCursor(std::int32_t column, std::int32_t row) {
    column_ = column;
    row_ = row;
}

void Text::putChar(char c) {
    if (c == '\n') {
        newline();
        return;
    }
    if (c == '\r') {
        column_ = 0;
        return;
    }
    if (columns() <= 0 || rows() <= 0) {
        return; // No font, unbound graphics or surface below one cell.
    }
    if (column_ >= columns()) {
        newline(); // Wrap at the right edge.
    }
    const std::int32_t cellX = column_ * font_->glyphWidth;
    const std::int32_t cellY = row_ * font_->glyphHeight;
    // Byte -> glyph index: identity for single-byte code pages (fonts
    // are complete datasets in native byte order; see CodePage).
    const std::uint16_t glyphIndex = static_cast<unsigned char>(c);
    if (glyphIndex < font_->glyphCount) {
        graphics_.drawBitmap(cellX, cellY, font_->glyph(glyphIndex),
                             foreground_, background_);
    } else {
        // Fallback glyph: a blank opaque cell keeps the grid coherent
        // when the font lacks the requested glyph.
        for (std::int32_t i = 0; i < font_->glyphHeight; ++i) {
            graphics_.drawHorizontalLine(cellX, cellY + i,
                                         font_->glyphWidth, background_);
        }
    }
    if (underline_ && font_->underlineRow != Font::kNoUnderlineRow &&
        font_->underlineRow < font_->glyphHeight) {
        graphics_.drawHorizontalLine(cellX, cellY + font_->underlineRow,
                                     font_->glyphWidth, foreground_);
    }
    ++column_;
}

void Text::print(const char* text) {
    if (text == nullptr) {
        return;
    }
    while (*text != '\0') {
        putChar(*text++);
    }
}

void Text::printf(const char* format, ...) {
    char buffer[kPrintfBufferSize];
    va_list args;
    va_start(args, format);
    const int written = std::vsnprintf(buffer, sizeof buffer, format, args);
    va_end(args);
    if (written < 0) {
        return; // Encoding error: the buffer contents are unspecified.
    }
    print(buffer);
}

void Text::setColors(Color foreground, Color background) {
    foreground_ = foreground;
    background_ = background;
}

void Text::setUnderline(bool enabled) {
    underline_ = enabled;
}

void Text::clear() {
    graphics_.clear(background_);
    column_ = 0;
    row_ = 0;
}

void Text::scroll(std::int32_t lines) {
    if (lines <= 0) {
        return;
    }
    const std::int32_t gridRows = rows();
    if (gridRows <= 0) {
        return;
    }
    const std::int32_t cellHeight = font_->glyphHeight;
    if (lines >= gridRows) {
        fillPixelRows(0, gridRows * cellHeight); // Everything scrolls out.
        return;
    }
    const std::int32_t keptRows = (gridRows - lines) * cellHeight;
    graphics_.copyRows(lines * cellHeight, 0, keptRows);
    fillPixelRows(keptRows, lines * cellHeight);
}

void Text::newline() {
    column_ = 0;
    ++row_;
}

void Text::fillPixelRows(std::int32_t y, std::int32_t pixelRows) {
    for (std::int32_t i = 0; i < pixelRows; ++i) {
        graphics_.drawHorizontalLine(0, y + i, graphics_.width(),
                                     background_);
    }
}

} // namespace picottl
