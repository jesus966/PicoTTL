// PicoTTL - Example 09: screen border
//
// Validates: exact visible-area geometry. A one-pixel border is drawn on
//   the outermost visible rows and columns (x = 0, x = width-1, y = 0,
//   y = height-1).
//
// Expected output: a closed one-pixel rectangle exactly at the edge of
//   the lit area; all four sides fully visible and unbroken. The visible
//   area is exactly 720x350.
//
// Regressions detected: off-by-one coordinate errors (a missing side),
//   active-window/display-enable misalignment (border cut off or a black
//   gap before it), clipping bugs at the extreme coordinates.
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
    const auto white = picottl::colors::kWhite;

    picottl::Graphics& gfx = display.graphics();
    gfx.clear();
    gfx.drawHorizontalLine(0, 0, w, white);     // top row
    gfx.drawHorizontalLine(0, h - 1, w, white); // bottom row
    gfx.drawVerticalLine(0, 0, h, white);       // leftmost column
    gfx.drawVerticalLine(w - 1, 0, h, white);   // rightmost column

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
