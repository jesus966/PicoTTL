// PicoTTL - Example 41: EGA terminal
//
// The Terminal layer on the EGA backend: the same unmodified Terminal
// state machine validated on MDA and CGA, now with default-palette
// cell attributes, a color block cursor and blinking colored text on
// the 80x25 grid of the 350-line raster.
//
// Screen:
//   - Header line, bright white on blue (cell attributes).
//   - A blinking alert line, light red on black (blank-cell blink
//     model, 1 s period).
//   - An underlined light cyan line (underline via glyph row 12).
//   - An auto-scrolling log: one line every 400 ms, cycling through
//     the 15 non-black default-palette foregrounds - including the
//     real EGA brown - demonstrating cell-buffer scroll with per-cell
//     attribute preservation.
//   - Block cursor (fg/bg color swap) blinking at the insertion
//     point; switches to underline style every 6 s.
// Expected: smooth scroll with each line keeping its own color, blink
//   toggling cleanly, cursor visible over any color combination.
// Regressions: colors bleeding between lines on scroll = cell
//   attribute path; invisible cursor = overlay re-render; torn scroll
//   = copyRows at 8 bpp.
//
// Wiring: identical to Examples 35-40 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, PRIMARY B/G/R = 16/17/18, SECONDARY b/g/r = 19/20/21,
//   GND DE-9 pin 1).
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/ega/Colors.hpp"
#include "picottl/fonts/IbmEga8x14.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmEga350;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer8::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

picottl::TerminalCell gCells[picottl::Terminal::cellsFor(
    picottl::modeWidth(kMode) / 8, picottl::modeHeight(kMode) / 14)];

} // namespace

int main() {
    // 130 MHz: the EGA 350-line raster shares MDA's pixel crystal
    // (integer divider 8, -0.043% - see Example 34).
    set_sys_clock_khz(130'000, true);

    picottl::PackedFramebuffer8 framebuffer(
        gFramebufferStorage, sizeof gFramebufferStorage,
        picottl::modeWidth(kMode), picottl::modeHeight(kMode));

    picottl::rp2350::EgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3; // GPIO 4 internal handshake, leave unwired.
    config.primaryBluePin = 16;
    config.primaryGreenPin = 17;
    config.primaryRedPin = 18;
    config.secondaryBluePin = 19;
    config.secondaryGreenPin = 20;
    config.secondaryRedPin = 21;
    config.framebuffer = &framebuffer;

    picottl::rp2350::EgaDisplayDevice device(config);
    picottl::Display display(device);

    const bool ok = display.begin(kMode);

    namespace ega = picottl::ega;
    picottl::Text text(display.graphics(), picottl::fonts::kIbmEga8x14);
    picottl::Terminal terminal(text, gCells, sizeof gCells / sizeof gCells[0]);

    terminal.clear();

    // Header: bright white on blue.
    terminal.setAttributes({ega::kWhite, ega::kBlue, false, false});
    terminal.printf("%-80s", " PicoTTL Example 41 - EGA terminal");

    // Blinking alert, light red.
    terminal.setAttributes({ega::kLightRed, ega::kBlack, false, true});
    terminal.print("\n BLINKING light red alert line\n");

    // Underlined light cyan.
    terminal.setAttributes({ega::kLightCyan, ega::kBlack, true, false});
    terminal.print(" underlined light cyan status line\n\n");

    // Log area: colored, auto-scrolling.
    terminal.setCursorStyle(picottl::CursorStyle::Block);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    std::uint32_t lineNumber = 0;
    std::uint32_t lastLineMs = 0;
    std::uint32_t lastStyleMs = 0;
    bool blockCursor = true;
    while (true) {
        const std::uint32_t now = to_ms_since_boot(get_absolute_time());
        terminal.update(now);

        if (now - lastLineMs >= 400) {
            lastLineMs = now;
            const int paletteIndex = static_cast<int>(1 + lineNumber % 15);
            const picottl::Color fg = ega::kDefaultPalette[paletteIndex];
            terminal.setAttributes({fg, ega::kBlack, false, false});
            terminal.printf("log line %5lu in palette color %2d (0x%02X)\n",
                            static_cast<unsigned long>(lineNumber),
                            paletteIndex, fg.index);
            ++lineNumber;
        }

        if (now - lastStyleMs >= 6000) {
            lastStyleMs = now;
            blockCursor = !blockCursor;
            terminal.setCursorStyle(blockCursor
                                        ? picottl::CursorStyle::Block
                                        : picottl::CursorStyle::Underline);
        }

        sleep_ms(10);
    }
}
