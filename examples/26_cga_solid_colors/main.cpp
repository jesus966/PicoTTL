// PicoTTL - Example 26: CGA solid colors
//
// First CGA example with pixel data: fills the whole 640x200 Packed4
// framebuffer with one solid color and cycles through all sixteen RGBI
// colors, two seconds each, in IBM color-number order (0..15). Black
// marks the start of the sequence.
//
// Validates: the 4-bit pixel path (ttl_pixels4 + 4 bpp DMA streaming),
//   all four color wires and their bit order (Blue, Green, Red,
//   Intensity from bluePin upward), and the verbatim Color semantics
//   (pixel value = IBM color number).
// Expected: 2 s each of black, blue, green, cyan, red, magenta, brown,
//   light gray, dark gray, light blue, light green, light cyan, light
//   red, light magenta, yellow, white. Color 6 shows as BROWN on a
//   genuine 5153 (the monitor darkens dark yellow internally); dark
//   yellow on clones without that circuit is also correct.
// Regressions: wrong hue order = swapped color wires; colors 8-15
//   identical to 0-7 = INTENSITY wire dead; vertical stripes or
//   flicker = 4 bpp packing/streaming fault.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// lines; TTL RGBI monitors present low-impedance inputs):
//   GPIO 2  -> HSYNC     (DE-9 pin 8)
//   GPIO 3  -> VSYNC     (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 16 -> BLUE      (DE-9 pin 5)
//   GPIO 17 -> GREEN     (DE-9 pin 4)
//   GPIO 18 -> RED       (DE-9 pin 3)
//   GPIO 19 -> INTENSITY (DE-9 pin 6)
//   GND     -> GND       (DE-9 pin 1 only)
//
// NOTE: this is the AUTHENTIC STANDALONE CGA ADAPTER pinout, not the
// PicoTTL reference connector (EGA superset, where DE-9 pin 6 is fed
// from GPIO 20). On the reference connector, drive CGA monitors with
// EgaDisplayDevice's CGA modes instead (see Example 38 and
// docs/architecture.md).
// Note the crossed wires: the DE-9 carries R,G,B on pins 3,4,5 while
// the GPIOs count B,G,R upward - this keeps Color{n} equal to the IBM
// color number forever.
//
// The onboard LED blinks slowly when video is running, fast on error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstddef>
#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/cga/Colors.hpp"
#include "picottl/rp2350/CgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmCga640;
constexpr std::uint32_t kMillisecondsPerColor = 2000;

/// The full cycle, spelled with the backend-scoped named constants
/// (optional conveniences over raw indices - see cga/Colors.hpp).
/// The order is the IBM color number, so this table is also the
/// on-screen sequence the observer checks against.
constexpr picottl::Color kColorCycle[] = {
    picottl::cga::kBlack,     picottl::cga::kBlue,
    picottl::cga::kGreen,     picottl::cga::kCyan,
    picottl::cga::kRed,       picottl::cga::kMagenta,
    picottl::cga::kBrown,     picottl::cga::kLightGray,
    picottl::cga::kDarkGray,  picottl::cga::kLightBlue,
    picottl::cga::kLightGreen, picottl::cga::kLightCyan,
    picottl::cga::kLightRed,  picottl::cga::kLightMagenta,
    picottl::cga::kYellow,    picottl::cga::kWhite,
};
constexpr std::size_t kColorCount =
    sizeof kColorCycle / sizeof kColorCycle[0];

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer4::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

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
    picottl::Graphics& gfx = display.graphics();

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

    std::size_t colorIndex = 0;
    bool ledOn = false;
    while (true) {
        gfx.clear(kColorCycle[colorIndex]);
        colorIndex = (colorIndex + 1u) % kColorCount;

        // Blink the LED while the color is displayed.
        const std::uint32_t blinkMs = ok ? 500 : 100;
        for (std::uint32_t elapsed = 0; elapsed < kMillisecondsPerColor;
             elapsed += blinkMs) {
#ifdef PICO_DEFAULT_LED_PIN
            ledOn = !ledOn;
            gpio_put(PICO_DEFAULT_LED_PIN, ledOn);
#endif
            sleep_ms(blinkMs);
        }
    }
}
