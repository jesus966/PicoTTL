// PicoTTL - Example 32: CGA terminal
//
// The Terminal layer on the CGA backend: the same unmodified Terminal
// state machine the MDA suite validated (Example 23), now with colored
// cell attributes, a color block cursor and blinking colored text.
//
// Screen:
//   - Header line, white on blue (cell attributes).
//   - A blinking alert line, light red on black (blank-cell blink
//     model, 1 s period).
//   - An underlined light cyan line (underline via the font's row 7).
//   - An auto-scrolling log: one line every 400 ms, cycling through
//     the 15 non-black foregrounds, demonstrating cell-buffer scroll
//     with per-cell attribute preservation.
//   - Block cursor (fg/bg swap - now a true color swap) blinking at
//     the insertion point; switches to underline style every 6 s.
// Expected: smooth scroll with each line keeping its own color, blink
//   toggling cleanly, cursor visible over any color combination.
// Regressions: colors bleeding between lines on scroll = cell
//   attribute path; invisible cursor = overlay re-render; torn scroll
//   = copyRows at 4 bpp.
//
// Wiring: identical to Examples 26-31 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, BLUE=16, GREEN=17, RED=18, INTENSITY=19, GND DE-9 1/2,
//   470 ohm series resistors only on HSYNC and VSYNC; connect other
//   monitor signals directly.)
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/cga/Colors.hpp"
#include "picottl/fonts/IbmCga8x8.hpp"
#include "picottl/rp2350/CgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmCga640;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer4::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

picottl::TerminalCell gCells[picottl::Terminal::cellsFor(
    picottl::modeWidth(kMode) / 8, picottl::modeHeight(kMode) / 8)];

} // namespace

int main() {
    // 142.8 MHz: jitter-free integer clock dividers for both CGA modes,
    // vertical ~59.76 Hz (slightly BELOW nominal - see Example 25).
    set_sys_clock_khz(142'800, true);

    picottl::PackedFramebuffer4 framebuffer(
        gFramebufferStorage, sizeof gFramebufferStorage,
        picottl::modeWidth(kMode), picottl::modeHeight(kMode));

    picottl::rp2350::CgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3; // GPIO 4 internal handshake, leave unwired.
    config.bluePin = 16;
    config.greenPin = 17;
    config.redPin = 18;
    config.intensityPin = 19;
    config.framebuffer = &framebuffer;

    picottl::rp2350::CgaDisplayDevice device(config);
    picottl::Display display(device);

    const bool ok = display.begin(kMode);

    namespace cga = picottl::cga;
    picottl::Text text(display.graphics(), picottl::fonts::kIbmCga8x8);
    picottl::Terminal terminal(text, gCells, sizeof gCells / sizeof gCells[0]);

    terminal.clear();

    // Header: white on blue.
    terminal.setAttributes({cga::kWhite, cga::kBlue, false, false});
    terminal.printf("%-80s", " PicoTTL Example 32 - CGA terminal");

    // Blinking alert, light red.
    terminal.setAttributes({cga::kLightRed, cga::kBlack, false, true});
    terminal.print("\n BLINKING light red alert line\n");

    // Underlined light cyan.
    terminal.setAttributes({cga::kLightCyan, cga::kBlack, true, false});
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
            const auto fg = static_cast<std::uint8_t>(1 + lineNumber % 15);
            terminal.setAttributes(
                {picottl::Color{fg}, cga::kBlack, false, false});
            terminal.printf("log line %5lu in color %2u\n",
                            static_cast<unsigned long>(lineNumber),
                            static_cast<unsigned>(fg));
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
