// PicoTTL - Example 38: EGA running CGA-compatible 200-line modes
//
// IBM EGA is hardware-compatible with CGA timings: the card scans at
// 15.7 kHz for 200-line modes and 21.85 kHz for 350-line modes, and
// the monitor selects by VSYNC polarity. This example exercises that
// compatibility on the EGA backend: it cycles through all three
// supported rasters every 6 seconds, drawing a 16-color staircase
// with the mode-appropriate palette.
//
//   1 notch  = IbmEga350  (640x350, kDefaultPalette, 130.0 MHz clock)
//   2 notches = IbmCga640 (640x200, kCga200Palette, 142.8 MHz clock)
//   3 notches = IbmCga320 (320x200, kCga200Palette, 142.8 MHz clock)
//
// Validates: the EGA backend serving the referenced CGA timing
//   entries, APPLICATION-SIDE RECLOCKING between the two crystal
//   domains (no single system clock gives both families jitter-free
//   dividers - the app calls set_sys_clock_khz between end() and
//   begin()), framebuffer replacement across visible sizes, and the
//   kCga200Palette mapping (in 200-line modes a standard 5154 reads
//   the connector as CGA: intensity = Secondary Green = bit 4).
// Expected: the same 16-bar staircase in all three modes between a
//   white top band (with the mode notches) and a light cyan bottom
//   band; no loss of sync beyond the monitor's mode-change settle
//   (200<->350 switches change the line rate AND VSYNC polarity, so a
//   visible re-lock is normal - unlike the instant 640<->320 switch).
// Regressions: no picture in 200-line modes only = VSYNC polarity
//   handling; bright bars (8-15) identical to dark ones in 200-line
//   modes = the kCga200Palette/Secondary-Green path; wrong colors
//   after a reclock = divider recomputation.
//
// Wiring: identical to Examples 35-37 (HSYNC=2, VSYNC=3, GPIO4
//   unwired, PRIMARY B/G/R = 16/17/18, SECONDARY b/g/r = 19/20/21,
//   GND DE-9 pin 1).
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
#include "picottl/ega/Colors.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

namespace {

using picottl::VideoMode;

constexpr VideoMode kModes[] = {VideoMode::IbmEga350, VideoMode::IbmCga640,
                                VideoMode::IbmCga320};
constexpr std::uint32_t kClockKhz[] = {130'000, 142'800, 142'800};
constexpr std::size_t kModeCount = sizeof kModes / sizeof kModes[0];
constexpr std::uint32_t kMillisecondsPerMode = 6000;

/// One storage block sized for the LARGEST mode; each mode gets its
/// own correctly-sized framebuffer view over it.
alignas(std::uint32_t) std::uint8_t gStorage
    [picottl::PackedFramebuffer8::bytesFor(
        picottl::modeWidth(VideoMode::IbmEga350),
        picottl::modeHeight(VideoMode::IbmEga350))];

picottl::PackedFramebuffer8 gFramebuffers[kModeCount] = {
    {gStorage, sizeof gStorage, picottl::modeWidth(kModes[0]),
     picottl::modeHeight(kModes[0])},
    {gStorage, sizeof gStorage, picottl::modeWidth(kModes[1]),
     picottl::modeHeight(kModes[1])},
    {gStorage, sizeof gStorage, picottl::modeWidth(kModes[2]),
     picottl::modeHeight(kModes[2])},
};

/// Mode-appropriate 16-entry palette: the default (power-on) palette
/// on the 350-line raster, the CGA-compatible values on 200-line
/// rasters (see ega/Colors.hpp).
const picottl::Color* paletteFor(VideoMode mode) {
    return mode == VideoMode::IbmEga350 ? picottl::ega::kDefaultPalette
                                        : picottl::ega::kCga200Palette;
}

/// The 16-bar staircase between a white top band (with mode-marker
/// notches) and a light cyan bottom band.
void drawPattern(picottl::Graphics& gfx, const picottl::Color* palette,
                 std::int32_t notchCount) {
    const std::int32_t w = gfx.width();
    const std::int32_t h = gfx.height();
    const std::int32_t bandH = 8;

    const std::int32_t barW = w / 16;
    for (std::int32_t i = 0; i < 16; ++i) {
        const std::int32_t x0 = i * barW;
        const std::int32_t x1 = i == 15 ? w : x0 + barW;
        for (std::int32_t x = x0; x < x1; ++x) {
            gfx.drawVerticalLine(x, bandH, h - 2 * bandH, palette[i]);
        }
    }
    for (std::int32_t row = 0; row < bandH; ++row) {
        gfx.drawHorizontalLine(0, row, w, palette[15]);          // white
        gfx.drawHorizontalLine(0, h - 1 - row, w, palette[11]);  // lt cyan
    }
    for (std::int32_t n = 0; n < notchCount; ++n) {
        for (std::int32_t row = 1; row < bandH - 1; ++row) {
            gfx.drawHorizontalLine(8 + n * 24, row, 12, palette[0]);
        }
    }
}

} // namespace

int main() {
    set_sys_clock_khz(kClockKhz[0], true);

    picottl::rp2350::EgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3; // GPIO 4 internal handshake, leave unwired.
    config.primaryBluePin = 16;
    config.primaryGreenPin = 17;
    config.primaryRedPin = 18;
    config.secondaryBluePin = 19;
    config.secondaryGreenPin = 20;
    config.secondaryRedPin = 21;
    config.framebuffer = &gFramebuffers[0];

    picottl::rp2350::EgaDisplayDevice device(config);
    picottl::Display display(device);

    bool ok = display.begin(kModes[0]);
    drawPattern(display.graphics(), paletteFor(kModes[0]), 1);

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

        // Mode switch through the public API. Crossing crystal domains
        // (350-line <-> 200-line) requires the application to reclock:
        // clock policy belongs to the application, and the engine
        // derives its dividers from the system clock at begin().
        modeIndex = (modeIndex + 1u) % kModeCount;
        display.end();
        set_sys_clock_khz(kClockKhz[modeIndex], true);
        ok = display.setFramebuffer(&gFramebuffers[modeIndex]) &&
             display.begin(kModes[modeIndex]);
        if (ok) {
            drawPattern(display.graphics(), paletteFor(kModes[modeIndex]),
                        static_cast<std::int32_t>(modeIndex) + 1);
        }
    }
}
