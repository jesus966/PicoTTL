// PicoTTL - Example 19: intensity bars
//
// Validates: the 2-bit packed pixel path electrically - the second
//   pixel output (INTENSITY, GPIO videoPin + 1) driven from a single
//   packed pixel stream (one state machine, one DMA stream; only the
//   PIO program differs from the monochrome path).
//
// Expected output: four full-height vertical bars, 180 pixels each,
//   drawn with pixel values 0..3 left to right:
//     value 0 (00) - black
//     value 1 (01) - normal (VIDEO only)
//     value 2 (10) - INTENSITY only: on a real 5151 typically invisible
//                    or very faint (the monitor mixes intensity with
//                    video); whatever it shows must be uniform
//     value 3 (11) - bright (VIDEO + INTENSITY)
//   Bars must be uniform with clean vertical boundaries.
//
// Regressions detected: INTENSITY wiring/mapping errors (bars 1 and 3
//   look identical), packed bit-order errors (bar order scrambled),
//   DMA stream errors at 2 bpp (diagonal tearing or horizontal
//   displacement), sync instability with the wider pixel stream.
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

// 2 bpp framebuffer storage (63 KB), owned by the application.
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
    gfx.clear();
    const std::int32_t barWidth = display.width() / 4;
    for (std::int32_t bar = 0; bar < 4; ++bar) {
        const picottl::Color color{static_cast<std::uint8_t>(bar)};
        for (std::int32_t x = bar * barWidth; x < (bar + 1) * barWidth; ++x) {
            gfx.drawVerticalLine(x, 0, display.height(), color);
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
