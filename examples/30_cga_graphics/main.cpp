// PicoTTL - Example 30: CGA color graphics
//
// The full Graphics pipeline in 16 colors: the same primitives the MDA
// suite validated (Examples 03-15), now drawing Color indices 0..15
// through the identical, unmodified Graphics code - the color-agnostic
// contract at work.
//
// Scene:
//   - Concentric rectangles across the whole screen, cycling colors
//     1..15 inward every 4 pixels (drawHorizontalLine/drawVerticalLine).
//   - An X of two diagonals over them, light cyan and light magenta
//     (drawLine, Bresenham).
//   - The Example 15 invader sprite: opaque yellow-on-blue in the
//     center panel (drawBitmap fg/bg), transparent light red in the
//     four corners, deliberately clipped off-screen (drawBitmap fg +
//     clipping).
// Expected: crisp rings in the repeating 15-color sequence, clean
//   diagonals, sprites in the stated colors, corner sprites cut by the
//   screen edges without artifacts.
// Regressions: ring colors wrong = index path; ragged diagonals =
//   Bresenham over Packed4; wrong sprite colors = drawBitmap color
//   plumbing; garbage at corners = clipping fault.
//
// Wiring: identical to Examples 26-29 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, BLUE=16, GREEN=17, RED=18, INTENSITY=19, GND DE-9 1/2,
//   470 ohm series resistors only on HSYNC and VSYNC; connect other
//   monitor signals directly.)
//
// The onboard LED blinks slowly when video is running, fast on error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/cga/Colors.hpp"
#include "picottl/rp2350/CgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmCga640;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer4::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

// The Example 15 invader, unchanged: bitmaps are shape masks; color is
// injected at draw time (framework invariant - design principle 20).
constexpr std::uint8_t kInvaderData[] = {
    0x0E, 0x00, // ....###....
    0x5F, 0x40, // .#.#####.#.
    0x7F, 0xC0, // .#########.
    0xDF, 0x60, // ##.#####.##
    0xFF, 0xE0, // ###########
    0x35, 0x80, // ..##.#.##..
    0x4A, 0x40, // .#..#.#..#.
    0xC0, 0x60, // ##.......##
};
constexpr picottl::MonochromeBitmap kInvader{kInvaderData, 11, 8, 0};

void drawScene(picottl::Graphics& gfx) {
    namespace cga = picottl::cga;
    const std::int32_t w = gfx.width();
    const std::int32_t h = gfx.height();

    gfx.clear(cga::kBlack);

    // Concentric rectangles, colors 1..15 cycling inward.
    std::uint8_t color = 1;
    for (std::int32_t inset = 0; inset < h / 2; inset += 4) {
        const std::int32_t x0 = inset;
        const std::int32_t y0 = inset;
        const std::int32_t x1 = w - 1 - inset;
        const std::int32_t y1 = h - 1 - inset;
        gfx.drawHorizontalLine(x0, y0, x1 - x0 + 1, picottl::Color{color});
        gfx.drawHorizontalLine(x0, y1, x1 - x0 + 1, picottl::Color{color});
        gfx.drawVerticalLine(x0, y0, y1 - y0 + 1, picottl::Color{color});
        gfx.drawVerticalLine(x1, y0, y1 - y0 + 1, picottl::Color{color});
        color = color == 15 ? 1 : static_cast<std::uint8_t>(color + 1);
    }

    // Diagonals over the rings.
    gfx.drawLine(0, 0, w - 1, h - 1, cga::kLightCyan);
    gfx.drawLine(0, h - 1, w - 1, 0, cga::kLightMagenta);

    // Center panel: opaque sprite, yellow on blue.
    const std::int32_t panelW = kInvader.width + 16;
    const std::int32_t panelH = kInvader.height + 16;
    const std::int32_t panelX = (w - panelW) / 2;
    const std::int32_t panelY = (h - panelH) / 2;
    for (std::int32_t y = 0; y < panelH; ++y) {
        gfx.drawHorizontalLine(panelX, panelY + y, panelW, cga::kBlue);
    }
    gfx.drawBitmap(panelX + 8, panelY + 8, kInvader, cga::kYellow,
                   cga::kBlue);

    // Corner sprites, transparent, deliberately clipped off-screen.
    gfx.drawBitmap(-5, -4, kInvader, cga::kLightRed);
    gfx.drawBitmap(w - 6, -4, kInvader, cga::kLightRed);
    gfx.drawBitmap(-5, h - 4, kInvader, cga::kLightRed);
    gfx.drawBitmap(w - 6, h - 4, kInvader, cga::kLightRed);
}

} // namespace

int main() {
    // 142.8 MHz: jitter-free integer clock dividers for both CGA modes,
    // vertical ~59.76 Hz (slightly BELOW nominal - see Example 25).
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
    drawScene(display.graphics());

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
