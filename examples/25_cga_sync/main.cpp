// PicoTTL - Example 25: IBM CGA display, black screen
//
// First CGA milestone: validates that the CGA backend starts and the
// monitor locks onto a stable 640x200 @ ~60 Hz raster (15.7 kHz
// horizontal, both syncs active-high), showing a black screen. No
// pixel data is generated yet - color wiring arrives with Example 26.
//
// Validates: CGA sync timing tables, active-high VSYNC (first mode to
//   use it), vertical front porch handling (first mode with one), and
//   the named-pin CGA configuration.
// Expected: the 5153 (or compatible RGBI monitor) locks with a steady,
//   centered black raster; no rolling or tearing.
// Regressions: rolling picture = vertical timing; horizontal collapse
//   or foldover = horizontal timing; no lock at all = sync polarity.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// lines; TTL RGBI monitors present low-impedance inputs like the 5151):
//   GPIO 2 -> HSYNC (DE-9 pin 8)
//   GPIO 3 -> VSYNC (DE-9 pin 9)
//   GND    -> GND   (DE-9 pin 1 only)
//
// The onboard LED blinks slowly when video is running, fast on error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/rp2350/CgaDisplayDevice.hpp"

int main() {
    // 142.8 MHz gives BOTH CGA modes jitter-free integer clock dividers
    // (pixel clocks 14.28 / 7.14 MHz, -0.27%) and allows 640<->320 mode
    // switching without reclocking the chip. The slightly LOW rate
    // matters: it puts the vertical refresh at ~59.76 Hz, just under
    // nominal, which aged monitors with a drifted (non-adjustable)
    // vertical free-run can still capture - rates above nominal (e.g.
    // the +0.57% a 144 MHz clock produces) failed to lock on a real
    // Samtron SC-431E. Clock policy belongs to the application.
    set_sys_clock_khz(142'800, true);

    picottl::rp2350::CgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3; // GPIO 4 becomes an internal handshake signal
                         // once a framebuffer is attached - leave it
                         // unwired in future examples too.

    picottl::rp2350::CgaDisplayDevice device(config);
    picottl::Display display(device);

    const bool ok = display.begin(picottl::VideoMode::IbmCga640);

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
