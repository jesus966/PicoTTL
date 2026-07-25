// PicoTTL - Example 21: pixel encoding diagnostic
//
// A hardware DIAGNOSTIC tool, not a graphics demo: six deterministic
// bands of repeating patterns exercise every 2-bit pixel value at every
// alignment that matters, so each class of encoding error produces a
// distinctive, immediately visible signature in exactly one band.
//
// Bands (top to bottom, separated by one bright line):
//   1. value = x & 3        1-px columns 0,1,2,3 repeating.
//      Breaks if: bit order WITHIN a 2-bit field is wrong (column
//      brightness order changes), or pixel clock/pairing drifts.
//   2. value = (x >> 1) & 3 2-px columns.
//      Breaks if: field boundaries are misplaced by one bit.
//   3. value = (x >> 2) & 3 4-px columns = exactly one byte per column.
//      Breaks if: byte-level packing or stride is wrong.
//   4. value = (x >> 4) & 3 16-px columns = exactly one 32-bit word.
//      Breaks if: DMA byte swap / word handling regresses (columns
//      reorder within each 64-px group).
//   5. value = (x + y) & 3  1-px diagonal stripes at 45 degrees.
//      Breaks if: any per-pixel packing error exists (catch-all; every
//      value borders every other value).
//   6. value = y & 3        1-px horizontal rows.
//      Breaks if: row addressing/stride is wrong (rows reorder or shear).
//
// Expected output: bands 1-4 show vertical columns cycling
// black / normal / faint-or-invisible / bright with the SAME left-to-
// right order in every band (only the column width changes: 1, 2, 4,
// 16 px); band 5 shows fine 45-degree stripes; band 6 shows fine
// horizontal stripes. All patterns perfectly stable.
//
// Wiring (direct GPIO connection to the DE-9 works; if using series
// resistors keep them small and MATCHED, <= ~100 ohm - the 5151 inputs
// are low-impedance summing networks):
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

std::uint8_t patternValue(int band, std::int32_t x, std::int32_t y) {
    switch (band) {
        case 0: return static_cast<std::uint8_t>(x & 3);         // field bit order
        case 1: return static_cast<std::uint8_t>((x >> 1) & 3);  // field boundary
        case 2: return static_cast<std::uint8_t>((x >> 2) & 3);  // byte packing
        case 3: return static_cast<std::uint8_t>((x >> 4) & 3);  // word/bswap
        case 4: return static_cast<std::uint8_t>((x + y) & 3);   // catch-all diagonal
        default: return static_cast<std::uint8_t>(y & 3);        // row addressing
    }
}

} // namespace

int main() {
    set_sys_clock_khz(130'000, true);

    picottl::PackedFramebuffer2 framebuffer(
        gFramebufferStorage, sizeof gFramebufferStorage,
        picottl::modeWidth(kMode), picottl::modeHeight(kMode));

    picottl::rp2350::MdaDisplayDevice::Config config;
    config.hsyncPin = 2; // VSYNC on GPIO 3; GPIO 4 internal, leave unwired.
    // Explicit per-bit pixel pin mapping (equivalent to videoPin = 19).
    config.pixelPins[0] = 19; // VIDEO
    config.pixelPins[1] = 20; // INTENSITY
    config.framebuffer = &framebuffer;

    picottl::rp2350::MdaDisplayDevice device(config);
    picottl::Display display(device);

    picottl::Graphics& gfx = display.graphics();
    const std::int32_t w = display.width();
    const std::int32_t h = display.height();
    const int bandCount = 6;
    const std::int32_t bandHeight = h / bandCount;

    for (std::int32_t y = 0; y < h; ++y) {
        const int band = static_cast<int>(y / bandHeight) < bandCount
                             ? static_cast<int>(y / bandHeight)
                             : bandCount - 1;
        const bool separator = (y % bandHeight) == 0 && y != 0;
        for (std::int32_t x = 0; x < w; ++x) {
            const std::uint8_t value =
                separator ? 3u : patternValue(band, x, y);
            gfx.drawPixel(x, y, picottl::Color{value});
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
