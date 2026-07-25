// PicoTTL - Example 27: CGA color bars
//
// The RGBI staircase: sixteen full-height vertical bars, one per IBM
// color number, 0 (black) at the left edge through 15 (white) at the
// right. The classic first test pattern of any color video system.
//
// Validates: Packed4 addressing WITHIN a scanline (two pixels per
//   byte, MSB-first nibble order, byte and word boundaries - Example
//   26 only ever wrote one value everywhere), bar edges (transition
//   sharpness across all four color wires switching at once), and the
//   IBM color-number ordering end to end.
// Expected: 16 crisp bars, each 40 pixels wide: black, blue, green,
//   cyan, red, magenta, brown (dark yellow on clones), light gray,
//   then the same hues brightened: dark gray, light blue, light
//   green, light cyan, light red, light magenta, yellow, white.
// Regressions: bars swapped in pairs = nibble order fault; ghost
//   columns at bar edges = pixel-clock/edge skew; hue order wrong =
//   swapped color wires; right half identical to left = INTENSITY dead.
//
// Wiring: identical to Example 26 (HSYNC=2, VSYNC=3, GPIO4 unwired,
//   BLUE=16, GREEN=17, RED=18, INTENSITY=19, GND to DE-9 1/2. Use 470 ohm
//   series resistors only on HSYNC and VSYNC; connect other signals directly.)
//
// The onboard LED blinks slowly when video is running, fast on error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/rp2350/CgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmCga640;
constexpr std::uint8_t kColorCount = 16;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer4::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

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
    picottl::Graphics& gfx = display.graphics();

    // One bar per IBM color number, left to right. Any remainder
    // pixels (none at 640/16) would join the last bar.
    const std::int32_t width = gfx.width();
    const std::int32_t barWidth = width / kColorCount;
    for (std::uint8_t color = 0; color < kColorCount; ++color) {
        const std::int32_t x0 = color * barWidth;
        const std::int32_t x1 =
            color == kColorCount - 1 ? width : x0 + barWidth;
        for (std::int32_t x = x0; x < x1; ++x) {
            gfx.drawVerticalLine(x, 0, gfx.height(), picottl::Color{color});
        }
    }

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
