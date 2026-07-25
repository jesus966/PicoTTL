// PicoTTL - Demonstration 96: IBM Monochrome Display Adapter (scenes).
//
// The demonstration scenes: text attributes, windows and menus, the
// CP437 character set, full-raster line art, text-mode animation and a
// simulated terminal session. Each scene draws itself, holds for a
// moment and returns; main.cpp sequences them with transitions.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstring>

#include "pico/stdlib.h"

#include "DemoDraw.hpp"
#include "DemoText.hpp"
#include "Intro.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/Terminal.hpp"
#include "picottl/Text.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"

namespace mda_demo {

namespace demo = picottl::demo;

/// Standard scene header: a bright title on row 0 over a rule.
inline void sceneHeader(picottl::Text& text, const char* title) {
    text.setColors(kBright, kBlack);
    text.clear();
    demo::printCentered(text, 0, title);
    text.setColors(kNormal, kBlack);
    text.setCursor(0, 1);
    for (std::int32_t i = 0; i < text.columns(); ++i) {
        text.putChar(static_cast<char>(0xC4)); // horizontal rule
    }
}

// ---------------------------------------------------------------------------
// Scene: text attributes - what the MDA attribute byte offered.
// ---------------------------------------------------------------------------
inline void sceneTextAttributes(picottl::Graphics&, picottl::Text& text) {
    sceneHeader(text, "T E X T   A T T R I B U T E S");

    text.setColors(kNormal, kBlack);
    demo::drawBox(text, 14, 4, 52, 17, demo::BoxStyle::Single);

    const std::int32_t col = 18;
    text.setColors(kNormal, kBlack);
    text.setCursor(col, 6);
    text.print("Normal      the workhorse of 80-column text");

    text.setColors(kBright, kBlack);
    text.setCursor(col, 8);
    text.print("Bright      the INTENSITY line, one wire away");

    text.setColors(kNormal, kBlack);
    text.setUnderline(true);
    text.setCursor(col, 10);
    text.print("Underlined  a hardware attribute on the MDA");
    text.setUnderline(false);

    text.setColors(kBright, kBlack);
    text.setUnderline(true);
    text.setCursor(col, 12);
    text.print("Both        bright and underlined combined");
    text.setUnderline(false);

    text.setColors(kBlack, kNormal);
    text.setCursor(col, 14);
    text.print(" Reverse video - black text on a lit field  ");

    text.setColors(kNormal, kBlack);
    demo::printCentered(text, 22,
                        "Every attribute of the 1981 adapter, rendered by "
                        "PicoTTL");

    // Blinking line, emulated with timed redraws (the terminal layer
    // shows the real per-cell blink attribute in a later scene).
    const char* blinkLine = "Blinking    the classic attention-getter";
    for (int phase = 0; phase < 8; ++phase) {
        text.setColors(kNormal, kBlack);
        text.setCursor(col, 16);
        if ((phase & 1) == 0) {
            text.print(blinkLine);
        } else {
            for (std::size_t i = 0; i < std::strlen(blinkLine); ++i) {
                text.putChar(' ');
            }
        }
        sleep_ms(450);
    }
    sleep_ms(800);
}

// ---------------------------------------------------------------------------
// Scene: windows and menus - the DOS-era text user interface.
// ---------------------------------------------------------------------------
inline void sceneWindows(picottl::Graphics&, picottl::Text& text) {
    // Shaded desktop background.
    text.setColors(kNormal, kBlack);
    text.clear();
    demo::fillTextRect(text, 0, 1, 80, 24, demo::kShadeMedium);
    text.setColors(kBright, kBlack);
    demo::fillTextRect(text, 0, 0, 80, 1, ' ');
    demo::printCentered(text, 0, "W I N D O W S   A N D   M E N U S");
    sleep_ms(600);

    // Window A: an information window.
    text.setColors(kNormal, kBlack);
    demo::fillTextRect(text, 6, 4, 34, 13, ' ');
    demo::drawBox(text, 6, 4, 34, 13, demo::BoxStyle::Single);
    text.setCursor(9, 4);
    text.print(" Information ");
    text.setCursor(8, 6);
    text.print("Text windows are drawn");
    text.setCursor(8, 7);
    text.print("with CP437 box characters");
    text.setCursor(8, 8);
    text.print("and shaded backgrounds -");
    text.setCursor(8, 9);
    text.print("no graphics mode needed.");
    text.setCursor(8, 11);
    text.print("The MDA made interfaces");
    text.setCursor(8, 12);
    text.print("like this the standard");
    text.setCursor(8, 13);
    text.print("look of PC software.");
    sleep_ms(1400);

    // Window B: a menu, overlapping A, with a drop shadow.
    demo::fillTextRect(text, 32, 21, 38, 1, ' ');  // shadow: bottom
    demo::fillTextRect(text, 68, 9, 2, 13, ' ');   // shadow: right
    text.setColors(kBright, kBlack);
    demo::fillTextRect(text, 30, 8, 38, 13, ' ');
    demo::drawBox(text, 30, 8, 38, 13, demo::BoxStyle::Double);
    text.setCursor(33, 8);
    text.print(" Main Menu ");

    const char* items[6] = {
        "  Run demonstration           ",
        "  Show text attributes        ",
        "  Display character set       ",
        "  Draw line art               ",
        "  Open terminal               ",
        "  Exit to system              ",
    };
    auto drawItems = [&](int selected) {
        for (int i = 0; i < 6; ++i) {
            if (i == selected) {
                text.setColors(kBlack, kBright); // selection bar
            } else {
                text.setColors(kNormal, kBlack);
            }
            text.setCursor(33, 10 + i);
            text.print(items[i]);
        }
    };
    // The selection bar walks the menu.
    for (int step = 0; step < 13; ++step) {
        drawItems(step % 6);
        sleep_ms(350);
    }
    drawItems(0);
    sleep_ms(900);
}

// ---------------------------------------------------------------------------
// Scene: the full CP437 character set from the 9x14 generator.
// ---------------------------------------------------------------------------
inline void sceneCharacterSet(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "C H A R A C T E R   S E T");
    const auto& font = picottl::fonts::kIbmMda9x14;

    text.setColors(kNormal, kBlack);
    demo::drawBox(text, 22, 5, 36, 12, demo::BoxStyle::Single);

    // Glyphs are drawn through the graphics layer directly so that
    // control codes (0x0A, 0x0D...) appear as their CP437 glyphs.
    for (int code = 0; code < 256; ++code) {
        const std::int32_t cellX = 24 + (code % 32);
        const std::int32_t cellY = 7 + (code / 32);
        g.drawBitmap(cellX * 9, cellY * 14,
                     font.glyph(static_cast<unsigned char>(code)),
                     (code / 32) % 2 == 0 ? kNormal : kBright);
        if ((code & 15) == 15) {
            sleep_ms(18); // gentle fill-in, row by row
        }
    }

    text.setColors(kNormal, kBlack);
    demo::printCentered(text, 19,
                        "256 characters, 9 x 14 pixels each - the IBM "
                        "character generator");
    demo::printCentered(text, 21,
                        "Box lines, shades and symbols made text-mode "
                        "interfaces possible");
    sleep_ms(4200);
}

// ---------------------------------------------------------------------------
// Scene: full-raster line art. The original adapter was text-only;
// PicoTTL exposes every pixel of the same 720x350 raster (as the
// Hercules card famously did on this very monitor).
// ---------------------------------------------------------------------------
inline void sceneLineArt(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "F U L L - R A S T E R   G R A P H I C S");

    // Left: radial burst.
    {
        const std::int32_t cx = 184;
        const std::int32_t cy = 184;
        const std::int32_t left = 40, right = 328, top = 48, bottom = 320;
        int i = 0;
        for (std::int32_t x = left; x <= right; x += 16, ++i) {
            g.drawLine(cx, cy, x, top, (i & 1) ? kBright : kNormal);
            sleep_ms(14);
        }
        for (std::int32_t y = top; y <= bottom; y += 16, ++i) {
            g.drawLine(cx, cy, right, y, (i & 1) ? kBright : kNormal);
            sleep_ms(14);
        }
        for (std::int32_t x = right; x >= left; x -= 16, ++i) {
            g.drawLine(cx, cy, x, bottom, (i & 1) ? kBright : kNormal);
            sleep_ms(14);
        }
        for (std::int32_t y = bottom; y >= top; y -= 16, ++i) {
            g.drawLine(cx, cy, left, y, (i & 1) ? kBright : kNormal);
            sleep_ms(14);
        }
    }

    // Right: two phase-shifted sine traces over an axis.
    {
        const std::int32_t x0 = 372, x1 = 700, cy = 184;
        g.drawHorizontalLine(x0, cy, x1 - x0, kNormal);
        std::int32_t prevY1 = cy, prevY2 = cy;
        for (std::int32_t x = x0; x <= x1; ++x) {
            const std::uint32_t angle =
                static_cast<std::uint32_t>((x - x0) * 512 / (x1 - x0));
            const std::int32_t y1 = cy - (demo::isin(angle) * 110) / 128;
            const std::int32_t y2 = cy - (demo::isin(angle + 85) * 70) / 128;
            if (x > x0) {
                g.drawLine(x - 1, prevY1, x, y1, kBright);
                g.drawLine(x - 1, prevY2, x, y2, kNormal);
            }
            prevY1 = y1;
            prevY2 = y2;
            if ((x & 7) == 0) {
                sleep_ms(2);
            }
        }
    }

    text.setColors(kNormal, kBlack);
    demo::printCentered(text, 24,
                        "252,000 individually addressable pixels on the "
                        "monochrome monitor");
    sleep_ms(3600);
}

// ---------------------------------------------------------------------------
// Scene: text-mode animation - a marquee and a bouncing block.
// ---------------------------------------------------------------------------
inline void sceneAnimation(picottl::Graphics&, picottl::Text& text) {
    sceneHeader(text, "T E X T - M O D E   A N I M A T I O N");

    text.setColors(kNormal, kBlack);
    demo::drawBox(text, 8, 3, 64, 16, demo::BoxStyle::Single);
    demo::drawBox(text, 8, 20, 64, 3, demo::BoxStyle::Single);

    const char* marquee =
        "  *** PICOTTL *** SMOOTH TEXT-MODE ANIMATION ON THE IBM "
        "MONOCHROME DISPLAY *** 80 COLUMNS * 25 ROWS * 9x14 CELLS ***";
    const std::int32_t marqueeLen =
        static_cast<std::int32_t>(std::strlen(marquee));
    const std::int32_t windowCols = 60;

    // Bouncing 2x1 block inside the upper frame (cells 9..70, 4..17).
    std::int32_t ballX = 12, ballY = 6, dx = 1, dy = 1;
    std::int32_t offset = 0;

    for (int frame = 0; frame < 150; ++frame) {
        // Marquee: redraw the visible window, shifted.
        text.setColors(kBright, kBlack);
        text.setCursor(10, 21);
        for (std::int32_t i = 0; i < windowCols; ++i) {
            text.putChar(marquee[(offset + i) % marqueeLen]);
        }
        ++offset;

        // Ball: erase, move, bounce, draw.
        text.setColors(kNormal, kBlack);
        text.setCursor(ballX, ballY);
        text.print("  ");
        ballX += dx * 2;
        ballY += dy;
        if (ballX <= 9 || ballX >= 68) {
            dx = -dx;
            ballX += dx * 2;
        }
        if (ballY <= 4 || ballY >= 17) {
            dy = -dy;
            ballY += dy;
        }
        text.setColors(kBright, kBlack);
        text.setCursor(ballX, ballY);
        text.putChar(demo::kBlock);
        text.putChar(demo::kBlock);

        sleep_ms(80);
    }
    sleep_ms(600);
}

// ---------------------------------------------------------------------------
// Scene: a simulated terminal session on the PicoTTL Terminal layer -
// real cell buffer, real blinking cursor, real auto-scroll.
// ---------------------------------------------------------------------------
inline void terminalType(picottl::Terminal& terminal, const char* s,
                         std::uint32_t charDelayMs) {
    for (const char* p = s; *p != '\0'; ++p) {
        terminal.putChar(*p);
        terminal.update(to_ms_since_boot(get_absolute_time()));
        sleep_ms(charDelayMs);
    }
}

inline void terminalIdle(picottl::Terminal& terminal, std::uint32_t ms) {
    const std::uint32_t start = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start < ms) {
        terminal.update(to_ms_since_boot(get_absolute_time()));
        sleep_ms(20);
    }
}

inline void sceneTerminal(picottl::Graphics&, picottl::Text& text) {
    static picottl::TerminalCell cells[picottl::Terminal::cellsFor(80, 25)];
    picottl::Terminal terminal(text, cells,
                               sizeof cells / sizeof cells[0]);

    terminal.setAttributes({kNormal, kBlack, false, false});
    terminal.clear();
    terminal.setCursorStyle(picottl::CursorStyle::Block);

    terminal.setAttributes({kBright, kBlack, false, false});
    terminal.print("PicoTTL Terminal - simulated session\n");
    terminal.setAttributes({kNormal, kBlack, false, false});
    terminal.print("Cell buffer, attributes, cursor overlay and "
                   "auto-scroll: all real.\n\n");
    terminalIdle(terminal, 900);

    terminal.print("C:\\>");
    terminalIdle(terminal, 700);
    terminalType(terminal, "ver\n", 90);
    terminal.print("PicoTTL Text Services  Version 1.00\n\n");
    terminalIdle(terminal, 700);

    terminal.print("C:\\>");
    terminalIdle(terminal, 600);
    terminalType(terminal, "dir\n", 90);
    terminal.print(" Volume in drive C is PICOTTL\n"
                   " Directory of C:\\DEMO\n\n");
    const char* listing[5] = {
        "INTRO    TXT     1,240  07-17-86   9:12a\n",
        "SCENES   TXT     8,452  07-17-86   9:14a\n",
        "MDA      DOC     4,096  07-17-86   9:20a\n",
        "DEMO96   EXE    24,576  07-17-86   9:31a\n",
        "        4 File(s)     38,364 bytes\n\n",
    };
    for (const char* line : listing) {
        terminal.print(line);
        terminalIdle(terminal, 180);
    }

    terminal.print("C:\\>");
    terminalIdle(terminal, 600);
    terminalType(terminal, "type mda.doc\n", 80);
    terminal.setAttributes({kBright, kBlack, false, false});
    terminal.print("\nTHE IBM MONOCHROME DISPLAY ADAPTER\n");
    terminal.setAttributes({kNormal, kBlack, false, false});
    terminal.print("\nIntroduced with the IBM PC in 1981, the MDA gave\n"
                   "the sharpest text of its era: an 80 x 25 grid with\n"
                   "a generous 9 x 14 character box on a 720 x 350\n"
                   "raster at 18.4 kHz - and attributes in hardware:\n");
    terminal.setAttributes({kBright, kBlack, false, false});
    terminal.print("bright, ");
    terminal.setAttributes({kNormal, kBlack, true, false});
    terminal.print("underline");
    terminal.setAttributes({kNormal, kBlack, false, false});
    terminal.print(" and ");
    terminal.setAttributes({kNormal, kBlack, false, true});
    terminal.print("blinking");
    terminal.setAttributes({kNormal, kBlack, false, false});
    terminal.print(" text.\n\n");
    terminalIdle(terminal, 2600);

    terminal.print("C:\\>");
    terminalIdle(terminal, 3200);
    terminal.setCursorStyle(picottl::CursorStyle::None);
}

} // namespace mda_demo
