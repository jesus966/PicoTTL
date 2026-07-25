// PicoTTL - Example 10: corner markers
//
// Validates: origin placement, reachability of all four extreme corners
//   and clipping behaviour. Each corner gets a filled 8x8 square exactly
//   in the corner plus a 17x17 cross centred ON the corner pixel - the
//   cross arms deliberately extend off-screen and must be clipped
//   silently.
//
// Expected output: four filled 8x8 squares, one flush in each corner,
//   each overlaid by the two visible arms of a cross. No artifacts along
//   any edge.
//
// Regressions detected: origin not at top-left (markers appear in the
//   wrong corners), clipping bugs (garbage pixels, wrapped lines on the
//   opposite side of the screen), coordinate sign errors.
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

void fillSquare(picottl::Graphics& gfx, std::int32_t x, std::int32_t y,
                std::int32_t size, picottl::Color color) {
    for (std::int32_t i = 0; i < size; ++i) {
        gfx.drawHorizontalLine(x, y + i, size, color);
    }
}

void cross(picottl::Graphics& gfx, std::int32_t cx, std::int32_t cy,
           std::int32_t arm, picottl::Color color) {
    // Arms may extend off-screen on purpose: clipping must handle it.
    gfx.drawHorizontalLine(cx - arm, cy, 2 * arm + 1, color);
    gfx.drawVerticalLine(cx, cy - arm, 2 * arm + 1, color);
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

    fillSquare(gfx, 0, 0, 8, white);         // top-left
    fillSquare(gfx, w - 8, 0, 8, white);     // top-right
    fillSquare(gfx, 0, h - 8, 8, white);     // bottom-left
    fillSquare(gfx, w - 8, h - 8, 8, white); // bottom-right

    cross(gfx, 0, 0, 8, white);
    cross(gfx, w - 1, 0, 8, white);
    cross(gfx, 0, h - 1, 8, white);
    cross(gfx, w - 1, h - 1, 8, white);

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
