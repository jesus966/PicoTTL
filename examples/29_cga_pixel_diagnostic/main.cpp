// PicoTTL - Example 29: CGA pixel diagnostic (4 bpp packing)
//
// The Packed4 counterpart of Example 21: six horizontal bands, each
// exercising one aspect of 4-bit pixel packing and streaming, with
// white separator lines between bands. Any systematic distortion
// identifies its own cause by which band shows it.
//
// Bands (top to bottom):
//   1. x & 15        - 1-pixel rainbow: nibble order within the byte.
//      Expect a fine repeating 16-color sequence, black at the left
//      edge of each repeat.
//   2. (x >> 1) & 15 - 2-pixel colums: BYTE boundaries (two pixels
//      per byte at 4 bpp). Expect the same rainbow, doubled width.
//   3. (x >> 3) & 15 - 8-pixel columns: WORD boundaries (eight pixels
//      per 32-bit DMA word) and the byte-swap. Expect the rainbow at
//      8x width, colors in 0..15 order left to right within each
//      128-pixel repeat.
//   4. (x + y) & 15  - diagonal rainbow: the catch-all; any stuck bit,
//      swapped nibble or dropped word breaks its smooth 45-degree flow.
//   5. y & 15        - horizontal stripes: row addressing and stride.
//   6. (x ^ y) & 1 ? white : black - 1x1 checkerboard in full white:
//      the harshest data-rate pattern at 4 bpp (all four wires plus
//      intensity toggling every pixel clock).
// Regressions: band 1 wrong but 2 right = nibble order; 2 wrong = byte
//   addressing; 3 wrong = word/bswap; 4 wavy = timing jitter; 5 wrong
//   = stride; 6 smeared = analog bandwidth (cable/monitor, not data).
//
// Wiring: identical to Examples 26-28 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, BLUE=16, GREEN=17, RED=18, INTENSITY=19, GND DE-9 1/2).
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

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer4::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

std::uint8_t bandValue(int band, std::int32_t x, std::int32_t y) {
    switch (band) {
        case 0:
            return static_cast<std::uint8_t>(x & 15);
        case 1:
            return static_cast<std::uint8_t>((x >> 1) & 15);
        case 2:
            return static_cast<std::uint8_t>((x >> 3) & 15);
        case 3:
            return static_cast<std::uint8_t>((x + y) & 15);
        case 4:
            return static_cast<std::uint8_t>(y & 15);
        default:
            return ((x ^ y) & 1) != 0 ? 15u : 0u;
    }
}

} // namespace

int main() {
    // 142.8 MHz: jitter-free integer clock dividers for both CGA modes,
    // vertical ~59.76 Hz. The slightly LOW rate is deliberate: monitors
    // with a drifted, non-adjustable vertical free-run capture rates
    // just under nominal but not above it (validated on a Samtron
    // SC-431E, which never locked at the 60.26 Hz a 144 MHz clock
    // yields - see Example 25).
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

    constexpr int kBands = 6;
    const std::int32_t width = gfx.width();
    const std::int32_t height = gfx.height();
    const std::int32_t bandHeight = height / kBands; // 33 rows each
    for (int band = 0; band < kBands; ++band) {
        const std::int32_t y0 = band * bandHeight;
        const std::int32_t y1 =
            band == kBands - 1 ? height : y0 + bandHeight;
        // Last row of each band (except the final one) is a white
        // separator line.
        for (std::int32_t y = y0; y < y1; ++y) {
            const bool separator = band != kBands - 1 && y == y1 - 1;
            for (std::int32_t x = 0; x < width; ++x) {
                const std::uint8_t value =
                    separator ? 15u : bandValue(band, x, y);
                gfx.drawPixel(x, y, picottl::Color{value});
            }
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
