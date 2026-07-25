// PicoTTL - Example 14: animation test
//
// Validates: continuous framebuffer updates while video generation stays
//   fully hardware-driven. The CPU only writes framebuffer memory; the
//   PIO/DMA pipeline is never touched after begin(). A 24x24 white
//   square bounces horizontally across the middle of the screen.
//
// Expected output: a white square moving smoothly left-to-right and
//   back, forever, over a stable background border. Minor horizontal
//   shear ("tearing") on the moving square is EXPECTED at this stage:
//   updates are not synchronized to the refresh (no double buffering
//   yet). The border must remain rock-solid.
//
// Regressions detected: raster instability during framebuffer writes
//   (bus contention artifacts), frame drift (square trail/ghosting that
//   accumulates), DMA restart glitches (full-screen flicker).
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
constexpr std::int32_t kSquareSize = 24;
constexpr std::int32_t kStep = 3;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::MonochromeFramebuffer::bytesFor(picottl::modeWidth(kMode),
                                              picottl::modeHeight(kMode))];

void fillSquare(picottl::Graphics& gfx, std::int32_t x, std::int32_t y,
                std::int32_t size, picottl::Color color) {
    for (std::int32_t i = 0; i < size; ++i) {
        gfx.drawHorizontalLine(x, y + i, size, color);
    }
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
    const auto black = picottl::colors::kBlack;

    picottl::Graphics& gfx = display.graphics();
    gfx.clear();
    // Static reference frame: it must stay perfectly stable while the
    // square animates.
    gfx.drawHorizontalLine(0, 0, w, white);
    gfx.drawHorizontalLine(0, h - 1, w, white);
    gfx.drawVerticalLine(0, 0, h, white);
    gfx.drawVerticalLine(w - 1, 0, h, white);

    const bool ok = display.begin(kMode);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    // Animation loop: the CPU only writes framebuffer pixels. Video
    // output continues untouched on PIO + DMA.
    std::int32_t x = 2;
    std::int32_t direction = kStep;
    const std::int32_t y = (h - kSquareSize) / 2;
    while (true) {
        fillSquare(gfx, x, y, kSquareSize, black); // erase
        x += direction;
        if (x <= 2 || x + kSquareSize >= w - 2) {
            direction = -direction;
            x += 2 * direction;
        }
        fillSquare(gfx, x, y, kSquareSize, white); // redraw
        sleep_ms(ok ? 16 : 100);
    }
}
