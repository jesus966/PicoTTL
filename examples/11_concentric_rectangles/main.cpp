// PicoTTL - Example 11: concentric rectangles
//
// Validates: rectangle composition from line primitives, symmetric
//   geometry and clipping. Nested borders are drawn every 12 pixels of
//   inset; the outermost "rectangle" starts off-screen (inset -24) and
//   must be clipped away cleanly.
//
// Expected output: evenly nested rectangles shrinking towards the screen
//   centre, all four sides of each rectangle unbroken; equal spacing on
//   opposite sides.
//
// Regressions detected: asymmetric line placement (unequal margins),
//   inclusive/exclusive length errors (open rectangle corners), clipping
//   bugs (artifacts from the off-screen rectangle), geometry distortion.
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
#include "picottl/rp2350/MdaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmMda;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::MonochromeFramebuffer::bytesFor(picottl::modeWidth(kMode),
                                              picottl::modeHeight(kMode))];

void drawRectangle(picottl::Graphics& gfx, std::int32_t left, std::int32_t top,
                   std::int32_t right, std::int32_t bottom, picottl::Color color) {
    gfx.drawHorizontalLine(left, top, right - left + 1, color);
    gfx.drawHorizontalLine(left, bottom, right - left + 1, color);
    gfx.drawVerticalLine(left, top, bottom - top + 1, color);
    gfx.drawVerticalLine(right, top, bottom - top + 1, color);
}

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

    const std::int32_t w = display.width();
    const std::int32_t h = display.height();
    const auto white = picottl::colors::kWhite;

    picottl::Graphics& gfx = display.graphics();
    gfx.clear();
    // From one off-screen rectangle (clipped) down to the smallest one
    // that still fits, every 12 pixels of inset.
    for (std::int32_t inset = -24; inset < h / 2 - 1; inset += 12) {
        drawRectangle(gfx, inset, inset, w - 1 - inset, h - 1 - inset, white);
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
