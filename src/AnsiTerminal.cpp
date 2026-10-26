// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// AnsiTerminal.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/AnsiTerminal.hpp"

namespace picottl {

namespace {
constexpr std::int32_t kTabWidth = 8;
}

AnsiTerminal::AnsiTerminal(Terminal& terminal, const AnsiPalette& palette)
    : terminal_(terminal), palette_(palette) {
    applyAttributes();
}

void AnsiTerminal::processByte(std::uint8_t byte) {
    switch (state_) {
        case State::Ground:
            if (byte == 0x1B) {
                state_ = State::Escape;
            } else if (byte < 0x20) {
                handleControl(byte);
            } else if (byte != 0x7F) {
                terminal_.putChar(static_cast<char>(byte)); // incl. CP437 high glyphs
            }
            break;
        case State::Escape:
            handleEscape(byte);
            break;
        case State::Csi:
            handleCsiByte(byte);
            break;
    }
}

void AnsiTerminal::processBytes(const std::uint8_t* data, std::size_t length) {
    if (data == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < length; ++i) {
        processByte(data[i]);
    }
}

void AnsiTerminal::setBellHandler(BellHandler handler, void* user) {
    bellHandler_ = handler;
    bellUser_ = user;
}

void AnsiTerminal::resetAttributes() {
    state_ = State::Ground;
    sgr_ = {};
    applyAttributes();
}

void AnsiTerminal::handleControl(std::uint8_t byte) {
    switch (byte) {
        case 0x07: // BEL: delegate (future buzzer); ignored otherwise.
            if (bellHandler_ != nullptr) {
                bellHandler_(bellUser_);
            }
            break;
        case 0x08: // BS: one cell left, no erase.
            if (terminal_.cursorX() > 0) {
                terminal_.setCursor(terminal_.cursorX() - 1,
                                    terminal_.cursorY());
            }
            break;
        case 0x09: { // HT: next multiple-of-8 tab stop (clamped).
            std::int32_t column =
                (terminal_.cursorX() / kTabWidth + 1) * kTabWidth;
            if (column > terminal_.columns() - 1) {
                column = terminal_.columns() - 1;
            }
            terminal_.setCursor(column, terminal_.cursorY());
            break;
        }
        case 0x0A: // LF
        case 0x0B: // VT: treated as LF (ECMA-48 / Linux console).
        case 0x0C: // FF: treated as LF.
            lineFeed();
            break;
        case 0x0D: // CR
            terminal_.setCursor(0, terminal_.cursorY());
            break;
        default: // Unknown controls are ignored.
            break;
    }
}

void AnsiTerminal::handleEscape(std::uint8_t byte) {
    switch (byte) {
        case '[': // CSI introducer
            state_ = State::Csi;
            parameterCount_ = 0;
            for (std::size_t i = 0; i < kMaxParameters; ++i) {
                parameters_[i] = 0;
            }
            parametersOverflowed_ = false;
            privateMarker_ = false;
            return;
        case '7': // DECSC
            saveCursor();
            break;
        case '8': // DECRC
            restoreCursor();
            break;
        case 'c': // RIS: full reset.
            resetAttributes();
            terminal_.clear();
            terminal_.setCursorStyle(CursorStyle::Block);
            break;
        default: // Unsupported escape: ignore.
            break;
    }
    state_ = State::Ground;
}

void AnsiTerminal::handleCsiByte(std::uint8_t byte) {
    if (byte >= '0' && byte <= '9') {
        if (parametersOverflowed_) {
            return; // Parameters beyond the buffer are discarded whole.
        }
        const std::size_t index =
            parameterCount_ == 0 ? 0 : parameterCount_ - 1;
        if (parameterCount_ == 0) {
            parameterCount_ = 1;
        }
        // Saturate instead of wrapping: no supported sequence needs
        // values above 16 bits, and hostile input must stay harmless.
        std::uint32_t value = parameters_[index] * 10u + (byte - '0');
        parameters_[index] = value > 0xFFFFu ? 0xFFFFu : value;
        return;
    }
    if (byte == ';') {
        if (parameterCount_ == 0) {
            parameterCount_ = 1; // an empty leading parameter
        }
        if (parameterCount_ < kMaxParameters) {
            ++parameterCount_;
        } else {
            // Further parameters do not fit: drop them entirely rather
            // than merging their digits into the last stored one.
            parametersOverflowed_ = true;
        }
        return;
    }
    if (byte == '?') {
        privateMarker_ = true;
        return;
    }
    if (byte >= 0x20 && byte <= 0x3F) {
        return; // Other parameter/intermediate bytes: ignored.
    }
    if (byte < 0x20) {
        return; // Controls inside CSI: ignored (simplification).
    }
    dispatchCsi(byte); // Final byte 0x40..0x7E.
    state_ = State::Ground;
}

std::uint32_t AnsiTerminal::parameterOr(std::size_t index,
                                        std::uint32_t fallback) const {
    if (index >= parameterCount_ || parameters_[index] == 0) {
        return fallback;
    }
    return parameters_[index];
}

void AnsiTerminal::dispatchCsi(std::uint8_t finalByte) {
    switch (finalByte) {
        case 'A': // CUU: cursor up
            terminal_.setCursor(terminal_.cursorX(),
                                terminal_.cursorY() -
                                    static_cast<std::int32_t>(parameterOr(0, 1)));
            break;
        case 'B': // CUD: cursor down
            terminal_.setCursor(terminal_.cursorX(),
                                terminal_.cursorY() +
                                    static_cast<std::int32_t>(parameterOr(0, 1)));
            break;
        case 'C': // CUF: cursor forward
            terminal_.setCursor(terminal_.cursorX() +
                                    static_cast<std::int32_t>(parameterOr(0, 1)),
                                terminal_.cursorY());
            break;
        case 'D': // CUB: cursor back
            terminal_.setCursor(terminal_.cursorX() -
                                    static_cast<std::int32_t>(parameterOr(0, 1)),
                                terminal_.cursorY());
            break;
        case 'H': // CUP
        case 'f': // HVP
            terminal_.setCursor(
                static_cast<std::int32_t>(parameterOr(1, 1)) - 1,
                static_cast<std::int32_t>(parameterOr(0, 1)) - 1);
            break;
        case 'J': // ED
            eraseInDisplay(parameterCount_ > 0 ? parameters_[0] : 0);
            break;
        case 'K': // EL
            eraseInLine(parameterCount_ > 0 ? parameters_[0] : 0);
            break;
        case 's': // ANSI.SYS save cursor
            saveCursor();
            break;
        case 'u': // ANSI.SYS restore cursor
            restoreCursor();
            break;
        case 'h': // SM / DECSET
            if (privateMarker_ && parameterOr(0, 0) == 25) {
                terminal_.setCursorStyle(CursorStyle::Block); // show cursor
            }
            break;
        case 'l': // RM / DECRST
            if (privateMarker_ && parameterOr(0, 0) == 25) {
                terminal_.setCursorStyle(CursorStyle::None); // hide cursor
            }
            break;
        case 'm': // SGR
            dispatchSgr();
            break;
        default: // Unsupported CSI: consumed and ignored (extensible here).
            break;
    }
}

void AnsiTerminal::dispatchSgr() {
    const std::size_t count = parameterCount_ == 0 ? 1 : parameterCount_;
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t value = parameters_[i];
        switch (value) {
            case 0: // reset
                sgr_ = {};
                break;
            case 1:
                sgr_.bold = true;
                break;
            case 22:
                sgr_.bold = false;
                break;
            case 4:
                sgr_.underline = true;
                break;
            case 24:
                sgr_.underline = false;
                break;
            case 5:
                sgr_.blink = true;
                break;
            case 25:
                sgr_.blink = false;
                break;
            case 7:
                sgr_.reverse = true;
                break;
            case 27:
                sgr_.reverse = false;
                break;
            case 39:
                sgr_.foregroundDefault = true;
                break;
            case 49:
                sgr_.backgroundDefault = true;
                break;
            case 38: // extended foreground: 38;5;n or 38;2;r;g;b
            case 48: // extended background
            {
                const bool isForeground = value == 38;
                std::size_t skip = 0;
                if (i + 1 < count && parameters_[i + 1] == 5) {
                    skip = 2;
                } else if (i + 1 < count && parameters_[i + 1] == 2) {
                    skip = 4;
                }
                i += skip;
                // Mapped to the default color: the palette model is
                // 16-color; extended colors degrade gracefully.
                if (isForeground) {
                    sgr_.foregroundDefault = true;
                } else {
                    sgr_.backgroundDefault = true;
                }
                break;
            }
            default:
                if (value >= 30 && value <= 37) {
                    sgr_.foregroundIndex = static_cast<std::uint8_t>(value - 30);
                    sgr_.foregroundDefault = false;
                } else if (value >= 90 && value <= 97) {
                    sgr_.foregroundIndex =
                        static_cast<std::uint8_t>(value - 90 + 8);
                    sgr_.foregroundDefault = false;
                } else if (value >= 40 && value <= 47) {
                    sgr_.backgroundIndex = static_cast<std::uint8_t>(value - 40);
                    sgr_.backgroundDefault = false;
                } else if (value >= 100 && value <= 107) {
                    sgr_.backgroundIndex =
                        static_cast<std::uint8_t>(value - 100 + 8);
                    sgr_.backgroundDefault = false;
                }
                break; // Unknown SGR values are ignored.
        }
    }
    applyAttributes();
}

void AnsiTerminal::applyAttributes() {
    std::uint8_t fgIndex = sgr_.foregroundIndex;
    if (sgr_.bold && fgIndex < 8) {
        fgIndex = static_cast<std::uint8_t>(fgIndex + 8); // bold = bright
    }
    Color foreground = sgr_.foregroundDefault
                           ? (sgr_.bold ? palette_.foreground[15]
                                    : palette_.defaultForeground)
                           : palette_.foreground[fgIndex];
    Color background = sgr_.backgroundDefault ? palette_.defaultBackground
                                              : palette_.background[sgr_.backgroundIndex];
    if (sgr_.reverse) {
        const Color swap = foreground;
        foreground = background;
        background = swap;
    }
    terminal_.setAttributes({foreground, background, sgr_.underline, sgr_.blink});
}

void AnsiTerminal::lineFeed() {
    // LF moves down one row (scrolling at the bottom) and keeps the
    // column; Terminal::putChar('\n') provides row advance + scroll.
    const std::int32_t column = terminal_.cursorX();
    terminal_.putChar('\n');
    terminal_.setCursor(column, terminal_.cursorY());
}

void AnsiTerminal::eraseInDisplay(std::uint32_t selector) {
    const std::int32_t column = terminal_.cursorX();
    const std::int32_t row = terminal_.cursorY();
    switch (selector) {
        case 0: // cursor to end of screen
            terminal_.clearToEndOfScreen();
            break;
        case 1: // start of screen to cursor
            for (std::int32_t r = 0; r < row; ++r) {
                terminal_.setCursor(0, r);
                terminal_.clearLine();
            }
            terminal_.setCursor(0, row);
            for (std::int32_t i = 0; i <= column; ++i) {
                terminal_.putChar(' ');
            }
            terminal_.setCursor(column, row);
            break;
        case 2: // whole screen (cursor position preserved)
        case 3:
            terminal_.clear();
            terminal_.setCursor(column, row);
            break;
        default:
            break;
    }
}

void AnsiTerminal::eraseInLine(std::uint32_t selector) {
    const std::int32_t column = terminal_.cursorX();
    const std::int32_t row = terminal_.cursorY();
    switch (selector) {
        case 0:
            terminal_.clearToEndOfLine();
            break;
        case 1: // start of line to cursor
            terminal_.setCursor(0, row);
            for (std::int32_t i = 0; i <= column; ++i) {
                terminal_.putChar(' ');
            }
            terminal_.setCursor(column, row);
            break;
        case 2:
            terminal_.clearLine();
            break;
        default:
            break;
    }
}

void AnsiTerminal::saveCursor() {
    savedColumn_ = terminal_.cursorX();
    savedRow_ = terminal_.cursorY();
    savedAttributes_ = terminal_.attributes();
    savedSgr_ = sgr_;
}

void AnsiTerminal::restoreCursor() {
    sgr_ = savedSgr_;
    terminal_.setCursor(savedColumn_, savedRow_);
    terminal_.setAttributes(savedAttributes_);
}

} // namespace picottl
