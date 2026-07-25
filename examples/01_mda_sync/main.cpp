// PicoTTL - Example 01: IBM MDA display, black screen
//
// Validates the framework foundation: the display starts, the monitor
// locks onto a stable 720x350 @ ~50 Hz raster, and shows a black screen.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// monitor signal wires directly):
//   GPIO 2 -> HSYNC (DE-9 pin 8)
//   GPIO 3 -> VSYNC (DE-9 pin 9)
//   GND    -> GND   (DE-9 pin 1 only on the shared reference connector)
//
// The onboard LED blinks slowly when video is running, fast on error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

int main() {
    // 130 MHz = 8 x 16.25 MHz: lets the backend derive a jitter-free MDA
    // pixel clock. Clock policy belongs to the application.
    set_sys_clock_khz(130'000, true);

    picottl::rp2350::MdaDisplayDevice::Config config;
    config.pioInstance = 0;
    config.stateMachine = 0;
    config.dmaDataChannel = 0;
    config.dmaControlChannel = 1;
    config.hsyncPin = 2; // VSYNC is on GPIO 3.

    picottl::rp2350::MdaDisplayDevice device(config);
    picottl::Display display(device);

    const bool ok = display.begin(picottl::VideoMode::IbmMda);

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
