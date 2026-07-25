// PicoTTL - Example 31: CGA text
//
// The text subsystem on the CGA backend: the same unmodified Text
// renderer that the MDA suite validated, now with the IBM CGA 8x8 font
// and 16 colors on the exact 80x25 grid (640/8, 200/8 - no partial
// cells).
//
// Screen:
//   - Header line, bright white on blue.
//   - One line per foreground color 1..15, each naming its IBM color
//     over black.
//   - Attribute sampler: normal, bright, underline (PicoTTL keeps it
//     functional although real CGA had none - drawn on glyph row 7),
//     inverse video, and colored-on-colored combinations.
//   - A printf-formatted status line with the grid geometry.
// Expected: crisp 8x8 glyphs, correct color names matching their
//   actual colors (the definitive color-order test), readable inverse
//   and underline.
// Regressions: color/name mismatch = index path; ragged glyphs = 4bpp
//   drawBitmap; wrong grid = Text geometry derivation.
//
// Wiring: identical to Examples 26-30 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, BLUE=16, GREEN=17, RED=18, INTENSITY=19, GND DE-9 1/2,
//   470 ohm series resistors only on HSYNC and VSYNC; connect other
//   monitor signals directly.)
//
// The onboard LED blinks slowly when video is running, fast on error.
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

constexpr const char* kColorNames[16] = {
    "black",      "blue",          "green",       "cyan",
    "red",        "magenta",       "brown",       "light gray",
    "dark gray",  "light blue",    "light green", "light cyan",
    "light red",  "light magenta", "yellow",      "white",
};

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

    // Header: bright white on blue, full width.
    text.setColors(cga::kWhite, cga::kBlue);
    text.printf("%-80s", " PicoTTL Example 31 - CGA text, IBM 8x8 font");

    // One line per color, named by itself (color-order self-test).
    for (int color = 1; color <= 15; ++color) {
        text.setColors(picottl::Color{static_cast<std::uint8_t>(color)},
                       cga::kBlack);
        text.printf("  color %2d  %s\n", color, kColorNames[color]);
    }

    // Attribute sampler.
    text.setCursor(0, 18);
    text.setColors(cga::kLightGray, cga::kBlack);
    text.print("  normal light gray on black\n");
    text.setColors(cga::kWhite, cga::kBlack);
    text.print("  bright white on black\n");
    text.setUnderline(true);
    text.setColors(cga::kLightCyan, cga::kBlack);
    text.print("  underlined light cyan (row 7 - CGA never could)\n");
    text.setUnderline(false);
    text.setColors(cga::kBlack, cga::kLightGray);
    text.print("  inverse video: black on light gray\n");
    text.setColors(cga::kYellow, cga::kRed);
    text.print("  yellow on red\n");

    text.setColors(cga::kLightGreen, cga::kBlack);
    text.printf("  %ldx%ld cells, %ldx%ld px, ok=%d",
                static_cast<long>(text.columns()),
                static_cast<long>(text.rows()),
                static_cast<long>(display.width()),
                static_cast<long>(display.height()), ok ? 1 : 0);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

    bool ledOn = false;
    while (true) {
#ifdef PICO_DEFAULT_LED_PIN
        ledOn = !ledOn;
        gpio_put(PICO_DEFAULT_LED_PIN, ledOn);
#endif
        sleep_ms(ok ? 1000 : 100);
    }
}
