// PicoTTL - Example 20: packed gradient
//
// Validates: 2-bit pixel packing and bit ordering at single-pixel
//   granularity. Every pixel is written with value (x + y) & 3, so all
//   four 2-bit values appear in every byte of the framebuffer and every
//   value borders every other value diagonally - the 2 bpp counterpart
//   of the Example 07 checkerboard.
//
// Expected output: a fine diagonal texture of 1-pixel stripes cycling
//   black / normal / intensity-only / bright, drifting one pixel per
//   row (45-degree pattern). From a distance it averages to a uniform
//   glow; up close the diagonal stripe order must be identical across
//   the whole screen.
//
// Regressions detected: packed field bit-order errors (stripe order
//   changes within bytes -> vertical banding every 4 pixels), byte or
//   word boundary errors (4/16-pixel-period artifacts), DMA byte-swap
//   regressions (16-pixel-wide column artifacts), off-by-one packing
//   (pattern phase flips).
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// lines: the 5151 inputs are low-impedance summing networks - larger
// values attenuate INTENSITY and mismatched values skew signal edges):
//   GPIO 2  -> HSYNC     (DE-9 pin 8)
//   GPIO 3  -> VSYNC     (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 19 -> VIDEO     (DE-9 pin 7)
//   GPIO 20 -> INTENSITY (DE-9 pin 6)
//   GND     -> GND       (DE-9 pin 1 only on the shared reference connector)
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
    [picottl::PackedFramebuffer2::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

} // namespace

int main() {
    set_sys_clock_khz(130'000, true);

    picottl::PackedFramebuffer2 framebuffer(
        gFramebufferStorage, sizeof gFramebufferStorage,
        picottl::modeWidth(kMode), picottl::modeHeight(kMode));

    picottl::rp2350::MdaDisplayDevice::Config config;
    config.hsyncPin = 2;  // VSYNC on GPIO 3; GPIO 4 internal, leave unwired.
    config.videoPin = 19; // INTENSITY on GPIO 20 (always videoPin + 1).
    config.framebuffer = &framebuffer;

    picottl::rp2350::MdaDisplayDevice device(config);
    picottl::Display display(device);

    picottl::Graphics& gfx = display.graphics();
    for (std::int32_t y = 0; y < display.height(); ++y) {
        for (std::int32_t x = 0; x < display.width(); ++x) {
            gfx.drawPixel(x, y,
                          picottl::Color{static_cast<std::uint8_t>((x + y) & 3)});
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
