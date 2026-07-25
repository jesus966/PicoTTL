// PicoTTL - Example 40: EGA text
//
// The text subsystem on the EGA backend: the same unmodified Text
// renderer validated on MDA and CGA, now with the authentic IBM EGA
// 8x14 font on the exact 80x25 grid of the 350-line raster (640/8,
// 350/14 - no partial cells).
//
// Screen:
//   - Header line, bright white on blue.
//   - One line per default-palette color 1..15, each naming its color
//     over black - including the REAL EGA brown (0x14), no monitor
//     trickery involved.
//   - A second column showing the same hue at its non-default
//     levels, demonstrating colors no CGA could show (e.g. three
//     more blues: 0x08, 0x09, 0x38).
//   - Attribute sampler: normal, bright, underline (glyph row 12),
//     inverse video, colored-on-colored.
//   - A printf-formatted status line with the grid geometry.
// Expected: crisp 8x14 glyphs (the same shapes as the MDA examples,
//   one column narrower), correct color names matching their actual
//   colors, readable inverse and underline.
// Regressions: color/name mismatch = index path or wiring; ragged
//   glyphs = 8 bpp drawBitmap; wrong grid = Text geometry derivation.
//
// Wiring: identical to Examples 35-39 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, PRIMARY B/G/R = 16/17/18, SECONDARY b/g/r = 19/20/21,
//   GND DE-9 pin 1).
//
// The onboard LED blinks slowly when video is running, fast on error.
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

constexpr const char* kColorNames[16] = {
    "black",      "blue",          "green",       "cyan",
    "red",        "magenta",       "brown",       "light gray",
    "dark gray",  "light blue",    "light green", "light cyan",
    "light red",  "light magenta", "yellow",      "white",
};

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

    // Header: bright white on blue, full width.
    text.setColors(ega::kWhite, ega::kBlue);
    text.printf("%-80s", " PicoTTL Example 40 - EGA text, IBM 8x14 font");

    // One line per default-palette color, named by itself, plus the
    // same primary hue at other secondary levels (EGA exclusives).
    for (int i = 1; i <= 15; ++i) {
        const picottl::Color color = ega::kDefaultPalette[i];
        text.setColors(color, ega::kBlack);
        text.printf("  %02X %-14s", color.index, kColorNames[i]);
        // Same primary bits with each extra secondary combination.
        const auto primaries = static_cast<std::uint8_t>(color.index & 0x07);
        for (std::uint8_t sec = 1; sec < 8; sec += 2) {
            const auto variant =
                static_cast<std::uint8_t>(primaries | (sec << 3));
            if (variant != color.index) {
                text.setColors(picottl::Color{variant}, ega::kBlack);
                text.printf(" %02X", variant);
            }
        }
        text.print("\n");
    }

    // Attribute sampler.
    text.setCursor(0, 18);
    text.setColors(ega::kLightGray, ega::kBlack);
    text.print("  normal light gray on black\n");
    text.setColors(ega::kWhite, ega::kBlack);
    text.print("  bright white on black\n");
    text.setUnderline(true);
    text.setColors(ega::kLightCyan, ega::kBlack);
    text.print("  underlined light cyan (glyph row 12)\n");
    text.setUnderline(false);
    text.setColors(ega::kBlack, ega::kLightGray);
    text.print("  inverse video: black on light gray\n");
    text.setColors(ega::kYellow, ega::kBrown);
    text.print("  yellow on brown (the EGA specialty)\n");

    text.setColors(ega::kLightGreen, ega::kBlack);
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
