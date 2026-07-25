// PicoTTL - Example 35: EGA solid colors
//
// First EGA example with pixel data: fills the whole 640x350 Packed6
// framebuffer with one solid color and cycles through ALL 64 EGA
// colors, 1.5 s each, in numeric order 0x00..0x3F. Black marks the
// start of the sequence.
//
// Validates: the 8-bit pixel path (ttl_pixels8 + one byte per pixel;
//   on this platform the scanout depth must divide the PIO's 32-bit
//   shift register, so the six EGA wires are fed by an 8-bit stream
//   whose top two bits are never driven), all six color wires, and
//   the verbatim Color semantics (pixel value = IBM EGA color value
//   in rgbRGB order: bit 0 = Primary Blue ... bit 5 = Secondary Red).
// Expected: 64 uniform fields. The sequence is binary-ordered, so the
//   first 8 values are the pure primary combinations (black, blue,
//   green, cyan, red, magenta, dark yellow, white), then each block of
//   8 adds one secondary combination: values 0x08-0x0F add Secondary
//   Blue (a faint blue lift), 0x10-0x17 add Secondary Green, 0x30-0x3F
//   end with the brightest set (0x3F = bright white). Value 0x14
//   (Secondary Green + Primary Red) is the classic EGA brown.
//   Primaries carry 2/3 amplitude, secondaries 1/3: four distinct
//   levels per gun must be visible across the cycle.
// Regressions: hue not matching the documented order = swapped color
//   wires; secondary blocks identical to their primary-only versions =
//   a dead secondary wire; only 2 levels per gun = primary/secondary
//   pair swapped; vertical structure in a field = 6 bpp streaming
//   fault.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// lines):
//   GPIO 2  -> HSYNC           (DE-9 pin 8)
//   GPIO 3  -> VSYNC           (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 16 -> PRIMARY BLUE    (DE-9 pin 5)
//   GPIO 17 -> PRIMARY GREEN   (DE-9 pin 4)
//   GPIO 18 -> PRIMARY RED     (DE-9 pin 3)
//   GPIO 19 -> SECONDARY BLUE  (DE-9 pin 7)
//   GPIO 20 -> SECONDARY GREEN (DE-9 pin 6)
//   GPIO 21 -> SECONDARY RED   (DE-9 pin 2)
//   GND     -> GND             (DE-9 pin 1)
// Note the crossed wires: the DE-9 carries r,R,G,B,g,b on pins
// 2,3,4,5,6,7 while the GPIOs count B,G,R,b,g,r upward - crossed once
// in the cable, Color{n} equals the IBM EGA color value forever.
//
// The onboard LED blinks slowly when video is running, fast on error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmEga350;
constexpr std::uint32_t kMillisecondsPerColor = 1500;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer8::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

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
    picottl::Graphics& gfx = display.graphics();

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

    std::uint8_t colorValue = 0;
    bool ledOn = false;
    while (true) {
        gfx.clear(picottl::Color{colorValue});
        colorValue = static_cast<std::uint8_t>((colorValue + 1u) & 0x3Fu);

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
