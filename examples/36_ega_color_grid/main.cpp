// PicoTTL - Example 36: EGA color grid
//
// All 64 EGA colors on screen at once: an 8x8 grid of rectangles in
// numeric order (0x00 top-left, row-major, 0x3F bottom-right), each
// cell separated by a thin black gap.
//
// Validates: Packed8 addressing WITHIN a scanline (one byte per pixel,
//   byte and word boundaries, DMA byte swap - Example 35 only ever
//   wrote one value everywhere), simultaneous display of the full
//   palette, and cell edge sharpness with all six wires switching.
// Expected: 8 rows of 8 cells. Row r, column c shows value r*8+c.
//   Row 0 = the eight primary-only combinations (black..white);
//   each following row adds secondary combinations; row 7 ends in
//   bright white (0x3F). Value 0x14 (row 2, column 4) is brown.
// Regressions: cells swapped within a row = byte-order fault; a
//   column of wrong hues = a stuck color wire; ragged cell edges =
//   skew between wires (check the direct signal wiring and 470 ohm sync
//   resistors).
//
// Wiring: identical to Example 35 (HSYNC=2, VSYNC=3, GPIO4 unwired,
//   PRIMARY B/G/R = 16/17/18, SECONDARY b/g/r = 19/20/21, GND DE-9
//   pin 1 - pin 2 is Secondary Red, NOT ground).
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

    gfx.clear(picottl::Color{0});
    const std::int32_t w = gfx.width();
    const std::int32_t h = gfx.height();
    const std::int32_t cellW = w / 8;  // 80 px
    const std::int32_t cellH = h / 8;  // 43 px (6 px slack joins the last row)
    constexpr std::int32_t kGap = 2;
    for (std::int32_t row = 0; row < 8; ++row) {
        for (std::int32_t col = 0; col < 8; ++col) {
            const auto value = static_cast<std::uint8_t>(row * 8 + col);
            const std::int32_t x0 = col * cellW + kGap;
            const std::int32_t y0 = row * cellH + kGap;
            const std::int32_t x1 = (col + 1) * cellW - kGap;
            const std::int32_t y1 =
                (row == 7 ? h : (row + 1) * cellH) - kGap;
            for (std::int32_t y = y0; y < y1; ++y) {
                gfx.drawHorizontalLine(x0, y, x1 - x0, picottl::Color{value});
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
