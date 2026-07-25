// PicoTTL - Example 15: bitmap rendering
//
// Validates: Graphics::drawBitmap() - the rendering path the future text
//   renderer builds on. An 11x8 sprite (stored MSB-first, packed rows,
//   flash resident) is drawn transparently over black, over a pre-filled
//   white panel in OPAQUE mode (white-on-black inverted), and partially
//   off-screen at all four corners to exercise clipping.
//
// Expected output: a large centred alien made of a 5x3 block
//   of sprites; below it, an inverted (black-on-white) sprite inside a
//   white panel; a quarter of a sprite peeking out of each screen
//   corner.
//
// Regressions detected: bitmap bit-order errors (mirrored/garbled
//   sprite), stride errors (sheared sprite), transparent/opaque mode
//   confusion (panel overwritten), bitmap clipping bugs (corner sprites
//   wrapping or crashing).
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

// 11x8 alien sprite (an original PicoTTL design), 1 bpp, MSB-first,
// packed rows (2 bytes per row).
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

void fillRect(picottl::Graphics& gfx, std::int32_t x, std::int32_t y,
              std::int32_t width, std::int32_t height, picottl::Color color) {
    for (std::int32_t i = 0; i < height; ++i) {
        gfx.drawHorizontalLine(x, y + i, width, color);
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

    // A 5x3 block of sprites in the centre (transparent over black).
    const std::int32_t blockW = 5 * (kInvader.width + 6) - 6;
    const std::int32_t blockH = 3 * (kInvader.height + 6) - 6;
    const std::int32_t blockX = (w - blockW) / 2;
    const std::int32_t blockY = (h - blockH) / 2 - 30;
    for (std::int32_t row = 0; row < 3; ++row) {
        for (std::int32_t col = 0; col < 5; ++col) {
            gfx.drawBitmap(blockX + col * (kInvader.width + 6),
                           blockY + row * (kInvader.height + 6),
                           kInvader, white);
        }
    }

    // Opaque mode: an inverted sprite (black on white) inside a white
    // panel; the panel must remain intact around the glyph.
    const std::int32_t panelW = kInvader.width + 16;
    const std::int32_t panelH = kInvader.height + 16;
    const std::int32_t panelX = (w - panelW) / 2;
    const std::int32_t panelY = blockY + blockH + 40;
    fillRect(gfx, panelX, panelY, panelW, panelH, white);
    gfx.drawBitmap(panelX + 8, panelY + 8, kInvader, black, white);

    // Clipping: a quarter of a sprite peeking out of each corner.
    gfx.drawBitmap(-5, -4, kInvader, white);
    gfx.drawBitmap(w - 6, -4, kInvader, white);
    gfx.drawBitmap(-5, h - 4, kInvader, white);
    gfx.drawBitmap(w - 6, h - 4, kInvader, white);

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
