// PicoTTL - host tests: AnsiTerminal
//
// Validates the incremental ANSI parser against a Terminal on the host:
// control characters, cursor movement, erase operations, SGR attribute
// mapping through a backend palette, save/restore, cursor visibility
// and robustness against unknown sequences - the automated counterpart
// of hardware Example 24.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstring>

#include "framework/test.hpp"
#include "picottl/AnsiTerminal.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/PackedFramebuffer.hpp"
#include "picottl/Terminal.hpp"
#include "picottl/Text.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"

using picottl::AnsiPalette;
using picottl::AnsiTerminal;
using picottl::Color;
using picottl::CursorStyle;

namespace {

constexpr std::uint16_t kW = 720;
constexpr std::uint16_t kH = 350;

AnsiPalette testPalette() {
    AnsiPalette palette;
    for (int i = 0; i < 16; ++i) {
        palette.foreground[i] =
            i == 0 ? Color{0} : (i < 8 ? Color{1} : Color{3});
        palette.background[i] = i == 0 ? Color{0} : Color{1};
    }
    palette.defaultForeground = Color{1};
    palette.defaultBackground = Color{0};
    return palette;
}

struct Fixture {
    std::uint8_t storage[picottl::PackedFramebuffer2::bytesFor(kW, kH)] = {};
    picottl::PackedFramebuffer2 fb{storage, sizeof storage, kW, kH};
    picottl::Graphics gfx{&fb};
    picottl::Text text{gfx, picottl::fonts::kIbmMda9x14};
    picottl::TerminalCell cells[picottl::Terminal::cellsFor(80, 25)];
    picottl::Terminal terminal{text, cells, sizeof cells / sizeof cells[0]};
    AnsiTerminal ansi{terminal, testPalette()};

    void feed(const char* sequence) {
        ansi.processBytes(reinterpret_cast<const std::uint8_t*>(sequence),
                          std::strlen(sequence));
    }
};

} // namespace

PICOTTL_TEST(ansiPrintablesAndControls) {
    Fixture f;
    f.feed("AB\rC");            // CR overwrites at column 0
    CHECK_EQ(f.cells[0].character, 'C');
    CHECK_EQ(f.cells[1].character, 'B');
    f.feed("\nX");              // LF keeps the column (was 1)
    CHECK_EQ(f.cells[1 * 80 + 1].character, 'X');
    f.feed("\x08\x08Y");        // BS twice, then overwrite
    CHECK_EQ(f.cells[1 * 80 + 0].character, 'Y');
    f.feed("\tT");              // HT to column 8
    CHECK_EQ(f.cells[1 * 80 + 8].character, 'T');
}

PICOTTL_TEST(ansiCursorMovementSequences) {
    Fixture f;
    f.feed("\x1B[10;20H");      // CUP row 10, col 20 (1-based)
    CHECK_EQ(f.terminal.cursorX(), 19);
    CHECK_EQ(f.terminal.cursorY(), 9);
    f.feed("\x1B[3A");          // up 3
    CHECK_EQ(f.terminal.cursorY(), 6);
    f.feed("\x1B[2B");          // down 2
    CHECK_EQ(f.terminal.cursorY(), 8);
    f.feed("\x1B[5C");          // right 5
    CHECK_EQ(f.terminal.cursorX(), 24);
    f.feed("\x1B[30D");         // left 30, clamped at 0
    CHECK_EQ(f.terminal.cursorX(), 0);
    f.feed("\x1B[999;999H");    // clamped to the grid
    CHECK_EQ(f.terminal.cursorX(), 79);
    CHECK_EQ(f.terminal.cursorY(), 24);
}

PICOTTL_TEST(ansiSgrMapsThroughThePalette) {
    Fixture f;
    f.feed("\x1B[1mB");          // bold -> bright
    CHECK_EQ(f.cells[0].attributes.foreground.index, 3);
    f.feed("\x1B[0m\x1B[31mr");  // dark red -> normal
    CHECK_EQ(f.cells[1].attributes.foreground.index, 1);
    f.feed("\x1B[91mR");         // bright red -> bright
    CHECK_EQ(f.cells[2].attributes.foreground.index, 3);
    f.feed("\x1B[0m\x1B[4mu");   // underline
    CHECK(f.cells[3].attributes.underline);
    f.feed("\x1B[24m\x1B[5mk");  // blink
    CHECK(f.cells[4].attributes.blink);
    f.feed("\x1B[0m\x1B[7mv");   // reverse: fg/bg swapped
    CHECK_EQ(f.cells[5].attributes.foreground.index, 0);
    CHECK_EQ(f.cells[5].attributes.background.index, 1);
    f.feed("\x1B[0mn");          // reset
    CHECK_EQ(f.cells[6].attributes.foreground.index, 1);
    CHECK(!f.cells[6].attributes.underline);
}

PICOTTL_TEST(ansiExtendedColorParametersAreSkipped) {
    Fixture f;
    // 256-color foreground followed by underline: the 5;196 payload
    // must not be misread as separate SGR codes.
    f.feed("\x1B[38;5;196;4mx");
    CHECK(f.cells[0].attributes.underline);
    CHECK_EQ(f.cells[0].attributes.foreground.index, 1); // degraded to default
}

PICOTTL_TEST(ansiEraseOperations) {
    Fixture f;
    f.feed("0123456789");
    f.feed("\x1B[5;1H");  // move away, then back
    f.feed("\x1B[1;5H");  // row 1, col 5
    f.feed("\x1B[K");     // EL 0: cursor to end of line
    CHECK_EQ(f.cells[3].character, '3');
    CHECK_EQ(f.cells[4].character, ' ');
    CHECK_EQ(f.cells[79].character, ' ');
    f.feed("\x1B[1K");    // EL 1: start of line to cursor
    CHECK_EQ(f.cells[0].character, ' ');
    CHECK_EQ(f.terminal.cursorX(), 4); // cursor preserved
    f.feed("\x1B[2;1HZZ\x1B[2J"); // ED 2: whole screen, cursor kept
    CHECK_EQ(f.cells[1 * 80 + 0].character, ' ');
    CHECK_EQ(f.terminal.cursorX(), 2);
    CHECK_EQ(f.terminal.cursorY(), 1);
}

PICOTTL_TEST(ansiSaveRestoreAndVisibility) {
    Fixture f;
    f.terminal.setCursorStyle(CursorStyle::Block);
    f.feed("\x1B[7;9H\x1B" "7");   // DECSC at (8,6)
    f.feed("\x1B[1;1H");
    f.feed("\x1B" "8");            // DECRC
    CHECK_EQ(f.terminal.cursorX(), 8);
    CHECK_EQ(f.terminal.cursorY(), 6);
    f.feed("\x1B[?25l");           // hide cursor
    CHECK(f.terminal.cursorStyle() == CursorStyle::None);
    f.feed("\x1B[?25h");           // show cursor
    CHECK(f.terminal.cursorStyle() == CursorStyle::Block);
}

PICOTTL_TEST(ansiUnknownSequencesAreConsumed) {
    Fixture f;
    f.feed("\x1B[999X\x1B[>1;2;3q\x1BQ"); // unknown CSI, unknown escape
    f.feed("ok");
    CHECK_EQ(f.cells[0].character, 'o');
    CHECK_EQ(f.cells[1].character, 'k');
}

PICOTTL_TEST(ansiParsingIsIncremental) {
    Fixture a;
    Fixture b;
    const char* sequence = "\x1B[2;3Hhello \x1B[1mW";
    // Whole string at once vs one byte at a time: identical results.
    a.feed(sequence);
    for (const char* p = sequence; *p != '\0'; ++p) {
        b.ansi.processByte(static_cast<std::uint8_t>(*p));
    }
    CHECK_EQ(std::memcmp(a.cells, b.cells, sizeof a.cells), 0);
    CHECK_EQ(std::memcmp(a.storage, b.storage, sizeof a.storage), 0);
}

PICOTTL_TEST(ansiHostileParameterInputIsHarmless) {
    Fixture f;
    // More parameters than the buffer holds: the excess is discarded
    // whole. In particular its digits must NOT merge into the last
    // stored parameter (a bold '1' would otherwise become e.g. 12345).
    f.feed("\x1B[0;0;0;0;0;0;0;1;2;3;4;5m");
    f.feed("x");
    CHECK_EQ(f.cells[0].character, 'x');
    CHECK(f.cells[0].attributes.foreground.index ==
          Color{3}.index); // parameter 8 == 1 -> bold survived intact.

    // Absurdly long digit strings saturate instead of wrapping around
    // to a small (and therefore meaningful) value.
    Fixture g;
    g.feed("\x1B[99999999999999999999m");
    g.feed("y"); // Saturated value is unknown-SGR: ignored, no crash.
    CHECK_EQ(g.cells[0].character, 'y');
}

