// PicoTTL - Example 18: text scrolling
//
// Validates: Text::clear(), Text::scroll() and the fast surface row-copy
//   path (MonochromeFramebuffer::copyRows / memmove). The screen fills
//   with numbered lines, then scrolls terminal-style forever: each new
//   line pushes everything up by one text row.
//
// Expected output: a full 80x25 screen of numbered lines scrolling
//   smoothly upwards, one row at a time (~6 rows per second). The top
//   line disappears cleanly, the new bottom line appears complete, the
//   line counter increases monotonically, and no residue is left in any
//   row. The raster itself stays rock solid during scrolling.
//
// Regressions detected: copyRows overlap/direction errors (smeared or
//   duplicated rows), exposed-area clearing errors (ghost glyph
//   fragments on the bottom row), cursor bookkeeping errors (new lines
//   drawn at the wrong row), performance regressions (visibly slow or
//   tearing scroll would indicate the per-pixel fallback is being used
//   instead of the memmove fast path).
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

    // Fill the whole grid with numbered lines.
    text.clear();
    long lineNumber = 0;
    for (std::int32_t r = 0; r < text.rows(); ++r) {
        text.setCursor(0, r);
        text.printf("Line %ld: the quick brown fox jumps over the lazy dog",
                    lineNumber++);
    }

    const bool ok = display.begin(kMode);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    // Terminal-style scrolling forever: content up one row, new line at
    // the bottom. The CPU only touches framebuffer memory.
    while (true) {
        sleep_ms(ok ? 150 : 100);
        text.scroll();
        text.setCursor(0, text.rows() - 1);
        text.printf("Line %ld: the quick brown fox jumps over the lazy dog",
                    lineNumber++);
    }
}
