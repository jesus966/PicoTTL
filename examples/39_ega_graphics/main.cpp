// PicoTTL - Example 39: EGA color graphics
//
// The full Graphics pipeline in 64 colors: the same primitives the MDA
// and CGA suites validated, drawing Color values 0x00..0x3F through
// the identical, unmodified Graphics code - the color-agnostic
// contract at work on a third backend.
//
// Scene:
//   - Concentric rectangles across the whole screen, stepping through
//     ALL 64 EGA values inward every 4 pixels (drawHorizontal/
//     VerticalLine) - a smooth brightness/hue spiral no real EGA
//     could show in one frame (16 palette registers; we have none).
//   - An X of two diagonals over them, light cyan and light magenta
//     (drawLine, Bresenham).
//   - The Example 15 invader sprite: opaque yellow-on-blue in the
//     center panel (drawBitmap fg/bg), transparent light red in the
//     four corners, deliberately clipped off-screen (clipping).
// Expected: crisp rings sweeping the full gamut, clean diagonals,
//   sprites in the stated colors, corner sprites cut cleanly by the
//   screen edges.
// Regressions: ring sequence wrong = index path; ragged diagonals =
//   Bresenham over Packed8; wrong sprite colors = drawBitmap color
//   plumbing; garbage at corners = clipping fault.
//
// Wiring: identical to Examples 35-38 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, PRIMARY B/G/R = 16/17/18, SECONDARY b/g/r = 19/20/21,
//   GND DE-9 pin 1).
//
// The onboard LED blinks slowly when video is running, fast on error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/ega/Colors.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmEga350;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer8::bytesFor(picottl::modeWidth(kMode),
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
    namespace ega = picottl::ega;
    const std::int32_t w = gfx.width();
    const std::int32_t h = gfx.height();

    gfx.clear(ega::kBlack);

    // Concentric rectangles, sweeping all 64 values inward.
    std::uint8_t value = 1;
    for (std::int32_t inset = 0; inset < h / 2; inset += 4) {
        const std::int32_t x0 = inset;
        const std::int32_t y0 = inset;
        const std::int32_t x1 = w - 1 - inset;
        const std::int32_t y1 = h - 1 - inset;
        gfx.drawHorizontalLine(x0, y0, x1 - x0 + 1, picottl::Color{value});
        gfx.drawHorizontalLine(x0, y1, x1 - x0 + 1, picottl::Color{value});
        gfx.drawVerticalLine(x0, y0, y1 - y0 + 1, picottl::Color{value});
        gfx.drawVerticalLine(x1, y0, y1 - y0 + 1, picottl::Color{value});
        value = static_cast<std::uint8_t>((value + 1u) & 0x3Fu);
        if (value == 0) {
            value = 1; // skip black so every ring stays visible
        }
    }

    // Diagonals over the rings.
    gfx.drawLine(0, 0, w - 1, h - 1, ega::kLightCyan);
    gfx.drawLine(0, h - 1, w - 1, 0, ega::kLightMagenta);

    // Center panel: opaque sprite, yellow on blue.
    const std::int32_t panelW = kInvader.width + 16;
    const std::int32_t panelH = kInvader.height + 16;
    const std::int32_t panelX = (w - panelW) / 2;
    const std::int32_t panelY = (h - panelH) / 2;
    for (std::int32_t y = 0; y < panelH; ++y) {
        gfx.drawHorizontalLine(panelX, panelY + y, panelW, ega::kBlue);
    }
    gfx.drawBitmap(panelX + 8, panelY + 8, kInvader, ega::kYellow,
                   ega::kBlue);

    // Corner sprites, transparent, deliberately clipped off-screen.
    gfx.drawBitmap(-5, -4, kInvader, ega::kLightRed);
    gfx.drawBitmap(w - 6, -4, kInvader, ega::kLightRed);
    gfx.drawBitmap(-5, h - 4, kInvader, ega::kLightRed);
    gfx.drawBitmap(w - 6, h - 4, kInvader, ega::kLightRed);
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
