// PicoTTL - Example 12: radiating lines
//
// Validates: Bresenham line drawing across every slope class - shallow,
//   steep, all four quadrants and both axis directions - by drawing
//   lines from the screen centre to points spaced along all four edges.
//
// Expected output: a symmetric "starburst" centred on the screen; all
//   lines start at the same centre pixel and end exactly on an edge;
//   opposite lines are mirror images of each other.
//
// Regressions detected: slope-dependent asymmetries in the line
//   algorithm (starburst visibly lopsided), endpoint errors (lines
//   stopping short of the edges or overshooting), octant-swap bugs
//   (kinked or doubled lines).
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
    const std::int32_t cx = w / 2;
    const std::int32_t cy = h / 2;
    const auto white = picottl::colors::kWhite;

    picottl::Graphics& gfx = display.graphics();
    gfx.clear();
    for (std::int32_t x = 0; x < w; x += 24) { // top and bottom edges
        gfx.drawLine(cx, cy, x, 0, white);
        gfx.drawLine(cx, cy, x, h - 1, white);
    }
    for (std::int32_t y = 0; y < h; y += 24) { // left and right edges
        gfx.drawLine(cx, cy, 0, y, white);
        gfx.drawLine(cx, cy, w - 1, y, white);
    }
    // The exact corners, explicitly.
    gfx.drawLine(cx, cy, w - 1, 0, white);
    gfx.drawLine(cx, cy, w - 1, h - 1, white);

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
