// PicoTTL - Example 34: IBM EGA display, black screen
//
// First EGA milestone: validates that the EGA backend starts and the
// monitor locks onto a stable 640x350 @ ~60 Hz raster (21.85 kHz
// horizontal), showing a black screen. No pixel data is generated
// yet - color wiring arrives with Example 35.
//
// Validates: the IbmEga350 timing table, the 21.85 kHz line rate, and
//   the sync polarities: HSYNC active-HIGH, VSYNC active-LOW. The
//   negative VSYNC is the signal a 5154 Enhanced Color Display uses
//   to select its 350-line mode (positive VSYNC = CGA-compatible
//   200-line mode), so correct polarity is the very first thing to
//   verify.
// Expected on a 5154-class monitor: steady, centered black raster in
//   350-line mode; no rolling or tearing.
// Expected on an oscilloscope (130 MHz system clock -> 16.25 MHz
//   pixel clock, -0.043% of the 16.257 MHz nominal):
//     HSYNC (GPIO 2): positive pulse, 3.94 us wide (64 px),
//       period 45.78 us  ->  21.84 kHz line rate.
//     VSYNC (GPIO 3): NEGATIVE pulse (idles high), 137.4 us wide
//       (3 lines), period 16.66 ms  ->  60.01 Hz refresh.
//     Blanking: 0.98 us front porch and 1.48 us back porch around
//       the HSYNC pulse.
// Regressions: rolling picture = vertical timing; horizontal collapse
//   or foldover = horizontal timing; monitor stuck in 200-line mode =
//   VSYNC polarity not active-low.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// monitor signal wires directly):
//   GPIO 2 -> HSYNC (DE-9 pin 8)
//   GPIO 3 -> VSYNC (DE-9 pin 9)
//   GND    -> GND   (DE-9 pin 1)
// CAUTION: on the EGA connector pin 2 is Secondary Red, NOT a ground
// (unlike CGA). Leave it unconnected in this example.
//
// The onboard LED blinks slowly when video is running, fast on error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

int main() {
    // 130 MHz = 8 x 16.25 MHz: the EGA 350-line raster shares MDA's
    // 16.257 MHz pixel crystal, so the MDA-proven clock gives a
    // jitter-free integer divider (-0.043% error, ~60.01 Hz vertical).
    // Clock policy belongs to the application.
    set_sys_clock_khz(130'000, true);

    picottl::rp2350::EgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3; // GPIO 4 becomes an internal handshake signal
                         // once a framebuffer is attached - leave it
                         // unwired in future examples too.

    picottl::rp2350::EgaDisplayDevice device(config);
    picottl::Display display(device);

    const bool ok = display.begin(picottl::VideoMode::IbmEga350);

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
