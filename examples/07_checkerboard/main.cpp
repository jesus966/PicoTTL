// PicoTTL - Example 07: pixel checkerboard
//
// Validates: the complete pixel path at single-pixel granularity -
//   MSB-first framebuffer layout, bit ordering within bytes, byte
//   boundaries, DMA byte swapping and pixel-clock integrity. This is
//   the definitive pixel-path validation.
//
// Expected output: a perfectly uniform full-screen 1-pixel checkerboard
//   (50% texture, looks like flat gray from a distance). Row 0 starts
//   with a lit pixel at (0,0).
//
// Regressions detected: swapped bit order (vertical banding every 8
//   pixels), missing/incorrect DMA byte swap (4-byte-wide column
//   artifacts), off-by-one pixel or line errors (pattern phase flips),
//   dropped or duplicated pixels (moire/tearing in the pattern).
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

    picottl::Graphics& gfx = display.graphics();
    gfx.clear();
    for (std::int32_t y = 0; y < display.height(); ++y) {
        for (std::int32_t x = 0; x < display.width(); ++x) {
            if (((x + y) & 1) == 0) {
                gfx.drawPixel(x, y, picottl::colors::kWhite);
            }
        }
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
