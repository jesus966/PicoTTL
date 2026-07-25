// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// AnsiTerminal.hpp
// ANSI/ECMA-48 escape-sequence interpreter over a Terminal.
//
// PUBLIC API - FROZEN (new sequences are added over time; existing
// behaviour never changes). Platform-independent.
//
// Layering (deliberate): Terminal remains a GENERIC text terminal with
// no ANSI knowledge, usable by applications that never need escape
// sequences. AnsiTerminal only parses incoming bytes and translates
// them into Terminal operations. Future VT100/VT220 compatibility will
// naturally extend THIS class - the rendering layers below never
// change.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>

#include "picottl/Terminal.hpp"

namespace picottl {

/// Backend-appropriate rendering of the 16 ANSI colors (8 dark + 8
/// bright) plus the defaults. The parser is completely backend
/// agnostic: it only selects entries from this palette, which the
/// application builds for its backend (e.g. on MDA: dark foregrounds
/// map to normal, bright foregrounds to VIDEO+INTENSITY).
struct AnsiPalette {
    Color foreground[16] = {};
    Color background[16] = {};
    Color defaultForeground{1};
    Color defaultBackground{0};
};

/// Incremental ANSI escape interpreter.
///
/// Consumes one byte at a time (no heap, no string buffering) and
/// drives a Terminal. Supported today: the common ASCII controls (BEL
/// BS HT LF VT FF CR), CSI cursor movement (CUU CUD CUF CUB CUP HVP),
/// screen operations (ED, EL), cursor save/restore (ESC 7/8 and CSI
/// s/u), cursor visibility (CSI ?25h/l), RIS (ESC c) and SGR attributes
/// (reset, bold, normal intensity, underline, blink, reverse, 16-color
/// and extended-color parameters). Unknown sequences are consumed and
/// ignored; adding new ones only requires new handlers.
///
/// Usage:
///   picottl::AnsiTerminal ansi(terminal, palette);
///   ansi.processByte(receivedByte); // e.g. from USB CDC
class AnsiTerminal {
public:
    /// Optional BEL (0x07) callback - e.g. a future buzzer.
    using BellHandler = void (*)(void* user);

    /// @param terminal Rendering terminal. Must outlive this object.
    /// @param palette  Backend-appropriate ANSI colors (copied).
    AnsiTerminal(Terminal& terminal, const AnsiPalette& palette);

    /// Feeds one received byte through the state machine.
    void processByte(std::uint8_t byte);

    /// Feeds a buffer of received bytes.
    void processBytes(const std::uint8_t* data, std::size_t length);

    /// Installs the BEL callback (nullptr = BEL is ignored).
    void setBellHandler(BellHandler handler, void* user);

    /// Resets parser state, attributes and cursor visibility (also
    /// performed by ESC c together with a screen clear).
    void resetAttributes();

private:
    static constexpr std::size_t kMaxParameters = 8;

    enum class State : std::uint8_t { Ground, Escape, Csi };

    void handleControl(std::uint8_t byte);
    void handleEscape(std::uint8_t byte);
    void handleCsiByte(std::uint8_t byte);
    void dispatchCsi(std::uint8_t finalByte);
    void dispatchSgr();
    void applyAttributes();
    void lineFeed();
    void eraseInDisplay(std::uint32_t selector);
    void eraseInLine(std::uint32_t selector);
    void saveCursor();
    void restoreCursor();
    std::uint32_t parameterOr(std::size_t index, std::uint32_t fallback) const;

    Terminal& terminal_;
    AnsiPalette palette_;

    State state_ = State::Ground;
    std::uint32_t parameters_[kMaxParameters] = {};
    std::size_t parameterCount_ = 0;
    /// True once a sequence carries more parameters than the buffer
    /// holds; the excess is discarded whole (values saturate at 0xFFFF)
    /// so hostile or malformed input can never corrupt stored ones.
    bool parametersOverflowed_ = false;
    bool privateMarker_ = false;

    // Logical SGR state (backend-independent).
    std::uint8_t foregroundIndex_ = 0;
    std::uint8_t backgroundIndex_ = 0;
    bool foregroundDefault_ = true;
    bool backgroundDefault_ = true;
    bool bold_ = false;
    bool underline_ = false;
    bool blink_ = false;
    bool reverse_ = false;

    std::int32_t savedColumn_ = 0;
    std::int32_t savedRow_ = 0;
    CellAttributes savedAttributes_{};

    BellHandler bellHandler_ = nullptr;
    void* bellUser_ = nullptr;
};

} // namespace picottl
