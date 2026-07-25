// PicoTTL - Example 16: single character
//
// Validates: the glyph rendering path (Font::glyph() + opaque
//   Graphics::drawBitmap()) and cell-based glyph positioning. One 'A'
//   is drawn at the exact centre cell of the 80x25 MDA text grid, and
//   one 'X' in each corner cell of the grid.
//
// Expected output: a white 'A' at the screen centre and an 'X' in each
//   of the four corners, each glyph exactly 9x14 pixels, crisp, inside
//   its cell (corner glyphs flush with the screen edges).
//
// Regressions detected: glyph bit-order or stride errors (garbled or
//   sheared character), cell-to-pixel conversion errors (misplaced
//   glyphs), font data corruption, opaque-background errors.
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
#include "picottl/fonts/IbmMda9x14.hpp"
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
    picottl::Text text(gfx, picottl::fonts::kIbmMda9x14);

    gfx.clear();

    // Centre cell of the 80x25 grid.
    text.setCursor(text.columns() / 2, text.rows() / 2);
    text.putChar('A');

    // The four corner cells.
    text.setCursor(0, 0);
    text.putChar('X');
    text.setCursor(text.columns() - 1, 0);
    text.putChar('X');
    text.setCursor(0, text.rows() - 1);
    text.putChar('X');
    text.setCursor(text.columns() - 1, text.rows() - 1);
    text.putChar('X');

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
