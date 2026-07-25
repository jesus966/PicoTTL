// PicoTTL - Example 13: deterministic graphics stress test
//
// Validates: the whole Graphics API under a fixed pseudo-random workload.
//   A xorshift32 generator with a FIXED seed draws points, lines and
//   rectangle borders; some primitives extend off-screen on purpose.
//   The output is bit-for-bit identical on every run and every firmware
//   version with a correct renderer.
//
// Expected output: a dense, static "scribble" of dots, lines and
//   rectangles. IDENTICAL every time the program runs - photograph the
//   screen once and compare against future versions pixel by pixel.
//
// Regressions detected: any behavioural change in drawPixel /
//   drawHorizontalLine / drawVerticalLine / drawLine / clipping, PRNG
//   accidental reseeding, framebuffer addressing changes - anything that
//   alters even one pixel changes the picture.
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

/// Deterministic PRNG (xorshift32). The seed is fixed by design.
class XorShift32 {
public:
    explicit XorShift32(std::uint32_t seed) : state_(seed) {}
    std::uint32_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_;
    }
    /// Uniform-ish value in [0, bound).
    std::int32_t below(std::int32_t bound) {
        return static_cast<std::int32_t>(next() % static_cast<std::uint32_t>(bound));
    }

private:
    std::uint32_t state_;
};

void drawRectangle(picottl::Graphics& gfx, std::int32_t left, std::int32_t top,
                   std::int32_t right, std::int32_t bottom, picottl::Color color) {
    gfx.drawHorizontalLine(left, top, right - left + 1, color);
    gfx.drawHorizontalLine(left, bottom, right - left + 1, color);
    gfx.drawVerticalLine(left, top, bottom - top + 1, color);
    gfx.drawVerticalLine(right, top, bottom - top + 1, color);
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

    XorShift32 rng(0x50C07751u); // Fixed seed - never change it.

    picottl::Graphics& gfx = display.graphics();
    gfx.clear();

    // 400 random points.
    for (int i = 0; i < 400; ++i) {
        gfx.drawPixel(rng.below(w), rng.below(h), white);
    }
    // 48 random lines; endpoints may fall slightly off-screen (clipping).
    for (int i = 0; i < 48; ++i) {
        gfx.drawLine(rng.below(w + 64) - 32, rng.below(h + 64) - 32,
                     rng.below(w + 64) - 32, rng.below(h + 64) - 32, white);
    }
    // 24 random rectangle borders.
    for (int i = 0; i < 24; ++i) {
        const std::int32_t left = rng.below(w - 16);
        const std::int32_t top = rng.below(h - 16);
        drawRectangle(gfx, left, top, left + 8 + rng.below(160),
                      top + 8 + rng.below(120), white);
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
