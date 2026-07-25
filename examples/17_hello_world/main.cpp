// PicoTTL - Example 17: Hello World
//
// Validates: the complete text pipeline - string rendering, cursor
//   advancement, newline handling, wrapping-free multi-line output and
//   printf formatting.
//
// Expected output, starting at the top-left cell:
//     Hello World
//     PicoTTL on IBM MDA: 80x25 cells, 720x350 pixels
//     printf: int=42 hex=0x2A str=MDA char=@
//   plus "The quick brown fox..." pangram in upper and lower case on
//   two further lines, and a final line showing the full printable
//   ASCII range.
//
// Regressions detected: cursor advancement errors (overlapping or
//   spaced-out glyphs), newline handling errors (lines misplaced),
//   printf formatting/truncation errors, font indexing errors
//   (wrong glyphs for correct codes).
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// lines: the 5151 inputs are low-impedance summing networks - larger
// values attenuate INTENSITY and mismatched values skew signal edges):
//   GPIO 2  -> HSYNC (DE-9 pin 8)
//   GPIO 3  -> VSYNC (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 19 -> VIDEO (DE-9 pin 7)
//   GND     -> GND   (DE-9 pin 1 only on the shared reference connector)
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmMda;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::MonochromeFramebuffer::bytesFor(picottl::modeWidth(kMode),
                                              picottl::modeHeight(kMode))];

} // namespace

int main() {
    set_sys_clock_khz(130'000, true);

    picottl::MonochromeFramebuffer framebuffer(
        gFramebufferStorage, sizeof gFramebufferStorage,
        picottl::modeWidth(kMode), picottl::modeHeight(kMode));

    picottl::rp2350::MdaDisplayDevice::Config config;
    config.hsyncPin = 2; // VSYNC on GPIO 3; GPIO 4 internal, leave unwired.
    config.videoPin = 19;
    config.framebuffer = &framebuffer;

    picottl::rp2350::MdaDisplayDevice device(config);
    picottl::Display display(device);

    picottl::Graphics& gfx = display.graphics();
    picottl::Text text(gfx, picottl::fonts::kIbmMda9x14);

    gfx.clear();
    text.setCursor(0, 0);
    text.print("Hello World\n");
    text.printf("PicoTTL on IBM MDA: %ldx%ld cells, %dx%d pixels\n",
                static_cast<long>(text.columns()),
                static_cast<long>(text.rows()),
                display.width(), display.height());
    text.printf("printf: int=%d hex=0x%02X str=%s char=%c\n", 42, 42, "MDA", '@');
    text.print("\nTHE QUICK BROWN FOX JUMPS OVER THE LAZY DOG 0123456789\n");
    text.print("the quick brown fox jumps over the lazy dog\n\n");
    for (char c = ' '; c < 127; ++c) {
        text.putChar(c); // Full printable ASCII, wrapping automatically.
    }

    const bool ok = display.begin(kMode);

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
        sleep_ms(ok ? 500 : 100);
    }
}
