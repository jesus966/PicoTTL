// PicoTTL - Example 37: EGA pixel diagnostic (8 bpp packing)
//
// The Packed8 counterpart of Examples 21 and 29: six horizontal bands,
// each exercising one aspect of byte-per-pixel packing and streaming,
// with bright white separator lines between bands. Any systematic
// distortion identifies its own cause by which band shows it.
//
// Bands (top to bottom):
//   1. x & 63         - 1-pixel rainbow: byte-level addressing. Expect
//      a fine 64-color sequence repeating every 64 px, black at each
//      repeat start.
//   2. (x >> 2) & 63  - 4-pixel columns: WORD boundaries (four pixels
//      per 32-bit DMA word at 8 bpp) and the byte swap. Expect the
//      rainbow at 4x width; any 4-pixel-periodic reordering means the
//      byte swap is wrong.
//   3. (x >> 4) & 63  - 16-pixel columns: coarse rainbow, values
//      0..39 visible per line span, easy to eyeball the 0..63 order.
//   4. (x + y) & 63   - diagonal rainbow: the catch-all; any stuck
//      wire, swapped byte or dropped word breaks its smooth flow.
//   5. y & 63         - horizontal stripes: row addressing and stride.
//   6. (x ^ y) & 1 ? 0x3F : 0 - 1x1 checkerboard in bright white: the
//      harshest data-rate pattern (all six wires toggling every pixel
//      clock).
// Regressions: band 1 wrong but 2 right = byte addressing; 2 wrong =
//   word/bswap; 4 wavy = timing jitter; 5 wrong = stride; 6 smeared
//   but 1-5 crisp = analog bandwidth (cable/monitor, not data).
//
// Wiring: identical to Examples 35-36 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, PRIMARY B/G/R = 16/17/18, SECONDARY b/g/r = 19/20/21,
//   GND DE-9 pin 1 - pin 2 is Secondary Red, NOT ground).
//
// The onboard LED blinks slowly when video is running, fast on error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmEga350;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer8::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

std::uint8_t bandValue(int band, std::int32_t x, std::int32_t y) {
    switch (band) {
        case 0:
            return static_cast<std::uint8_t>(x & 63);
        case 1:
            return static_cast<std::uint8_t>((x >> 2) & 63);
        case 2:
            return static_cast<std::uint8_t>((x >> 4) & 63);
        case 3:
            return static_cast<std::uint8_t>((x + y) & 63);
        case 4:
            return static_cast<std::uint8_t>(y & 63);
        default:
            return ((x ^ y) & 1) != 0 ? 0x3Fu : 0x00u;
    }
}

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
    picottl::Graphics& gfx = display.graphics();

    constexpr int kBands = 6;
    const std::int32_t width = gfx.width();
    const std::int32_t height = gfx.height();
    const std::int32_t bandHeight = height / kBands; // 58 rows each
    for (int band = 0; band < kBands; ++band) {
        const std::int32_t y0 = band * bandHeight;
        const std::int32_t y1 =
            band == kBands - 1 ? height : y0 + bandHeight;
        // Last row of each band (except the final one) is a bright
        // white separator line.
        for (std::int32_t y = y0; y < y1; ++y) {
            const bool separator = band != kBands - 1 && y == y1 - 1;
            for (std::int32_t x = 0; x < width; ++x) {
                const std::uint8_t value =
                    separator ? 0x3Fu : bandValue(band, x, y);
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
