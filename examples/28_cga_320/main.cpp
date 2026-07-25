// PicoTTL - Example 28: CGA 320x200 mode and 640<->320 switching
//
// Exercises the second CGA raster: 320x200 at half the pixel clock
// (7.16 MHz nominal) with the same sync frequencies as 640x200. The
// example draws the 16-bar RGBI staircase and switches between the
// two modes every five seconds through the public API
// (end -> setFramebuffer -> begin), without reclocking the RP2350.
//
// Validates: the IbmCga320 timing table, the half-rate pixel clock
//   (integer divider 10 at 144 MHz), runtime mode switching on the
//   CGA backend, and framebuffer replacement between visible sizes.
// Expected: the staircase between a WHITE top band (with mode-marker
//   notches: 1 notch = 320x200, 2 notches = 640x200) and a LIGHT CYAN
//   bottom band, identical on-screen geometry in both modes; no loss
//   of sync at the switch.
// Diagnostic reading (if something looks wrong):
//   - White band not at the top: note WHERE it appears and in which
//     mode (notches tell you) - a content wrap points at the pixel
//     stream restart, whole-raster displacement at vertical timing.
//   - Diagonal stripes: note whether they drift or stay locked, and
//     whether they follow one mode only. Stripes NOT locked to the
//     picture are electrical (supply/ground/cable), locked ones are
//     data-path.
// Regressions: loss of lock on switch = engine stop()/start() fault;
//   bars half the screen wide in 320 mode = pixel clock not halved.
//
// Wiring: identical to Examples 26/27 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, BLUE=16, GREEN=17, RED=18, INTENSITY=19, GND DE-9 1/2).
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
#include "picottl/rp2350/CgaDisplayDevice.hpp"

namespace {

constexpr picottl::VideoMode kModes[] = {picottl::VideoMode::IbmCga320,
                                         picottl::VideoMode::IbmCga640};
constexpr std::uint32_t kMillisecondsPerMode = 5000;
constexpr std::uint8_t kColorCount = 16;

/// One storage block sized for the LARGER mode; each mode gets its own
/// correctly-sized framebuffer view over it (same technique as the
/// diagnostics application).
alignas(std::uint32_t) std::uint8_t gStorage
    [picottl::PackedFramebuffer4::bytesFor(
        picottl::modeWidth(picottl::VideoMode::IbmCga640),
        picottl::modeHeight(picottl::VideoMode::IbmCga640))];

picottl::PackedFramebuffer4 gFramebuffers[] = {
    {gStorage, sizeof gStorage, picottl::modeWidth(kModes[0]),
     picottl::modeHeight(kModes[0])},
    {gStorage, sizeof gStorage, picottl::modeWidth(kModes[1]),
     picottl::modeHeight(kModes[1])},
};

/// Diagnostic pattern: RGBI staircase between two reference bands.
/// - TOP band (8 rows): WHITE, carrying black notch markers that
///   identify the mode unambiguously: 1 notch = 320x200, 2 = 640x200.
/// - BOTTOM band (8 rows): LIGHT CYAN.
/// If the picture content ever appears vertically displaced, the two
/// distinct bands reveal both the direction and the amount of the
/// wrap; if instead the whole raster moved on the tube, the bands
/// stay at the edges of the visible area.
void drawPattern(picottl::Graphics& gfx, bool is320) {
    const std::int32_t width = gfx.width();
    const std::int32_t height = gfx.height();
    const std::int32_t bandHeight = 8;
    const picottl::Color white{15};
    const picottl::Color lightCyan{11};

    // Middle: the RGBI staircase.
    const std::int32_t barWidth = width / kColorCount;
    for (std::uint8_t color = 0; color < kColorCount; ++color) {
        const std::int32_t x0 = color * barWidth;
        const std::int32_t x1 =
            color == kColorCount - 1 ? width : x0 + barWidth;
        for (std::int32_t x = x0; x < x1; ++x) {
            gfx.drawVerticalLine(x, bandHeight, height - 2 * bandHeight,
                                 picottl::Color{color});
        }
    }

    // Reference bands.
    for (std::int32_t row = 0; row < bandHeight; ++row) {
        gfx.drawHorizontalLine(0, row, width, white);
        gfx.drawHorizontalLine(0, height - 1 - row, width, lightCyan);
    }

    // Mode marker notches in the top band: 1 = 320x200, 2 = 640x200.
    const std::int32_t notchCount = is320 ? 1 : 2;
    for (std::int32_t n = 0; n < notchCount; ++n) {
        for (std::int32_t row = 1; row < bandHeight - 1; ++row) {
            gfx.drawHorizontalLine(8 + n * 24, row, 12, picottl::Color{0});
        }
    }
}

} // namespace

int main() {
    // 142.8 MHz serves BOTH modes with integer dividers (5 and 10):
    // switching never reclocks the chip. Vertical ~59.76 Hz, slightly
    // BELOW nominal so aged monitors still capture (see Example 25).
    set_sys_clock_khz(142'800, true);

    picottl::rp2350::CgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3; // GPIO 4 internal handshake, leave unwired.
    config.bluePin = 16;
    config.greenPin = 17;
    config.redPin = 18;
    config.intensityPin = 19;
    config.framebuffer = &gFramebuffers[0];

    picottl::rp2350::CgaDisplayDevice device(config);
    picottl::Display display(device);

    bool ok = display.begin(kModes[0]);
    drawPattern(display.graphics(),
                kModes[0] == picottl::VideoMode::IbmCga320);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
#endif

    std::size_t modeIndex = 0;
    bool ledOn = false;
    std::uint32_t elapsedMs = 0;
    while (true) {
        const std::uint32_t blinkMs = ok ? 500 : 100;
#ifdef PICO_DEFAULT_LED_PIN
        ledOn = !ledOn;
        gpio_put(PICO_DEFAULT_LED_PIN, ledOn);
#endif
        sleep_ms(blinkMs);
        elapsedMs += blinkMs;
        if (elapsedMs < kMillisecondsPerMode) {
            continue;
        }
        elapsedMs = 0;

        // Mode switch through the public API only.
        modeIndex = (modeIndex + 1u) % (sizeof kModes / sizeof kModes[0]);
        display.end();
        ok = display.setFramebuffer(&gFramebuffers[modeIndex]) &&
             display.begin(kModes[modeIndex]);
        if (ok) {
            drawPattern(display.graphics(),
                        kModes[modeIndex] == picottl::VideoMode::IbmCga320);
        }
    }
}
