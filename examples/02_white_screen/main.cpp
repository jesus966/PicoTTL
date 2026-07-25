// PicoTTL - Example 02: white screen
//
// Validates the complete pixel path: a monochrome framebuffer is scanned
// out continuously by the display device. The monitor should show a
// fully lit 720x350 area with black borders (blanking).
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// monitor signal wires directly):
//   GPIO 2  -> HSYNC (DE-9 pin 8)
//   GPIO 3  -> VSYNC (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 19 -> VIDEO (DE-9 pin 7)
//   GND     -> GND   (DE-9 pin 1 only on the shared reference connector)
//
// The onboard LED blinks slowly when video is running, fast on error.
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

// Framebuffer storage (31.5 KB), owned by the application. Backends scan
// it out by DMA, hence the 4-byte alignment.
alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::MonochromeFramebuffer::bytesFor(picottl::modeWidth(kMode),
                                              picottl::modeHeight(kMode))];

} // namespace

int main() {
    // 130 MHz = 8 x 16.25 MHz: lets the backend derive a jitter-free MDA
    // pixel clock. Clock policy belongs to the application.
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

    display.surface()->fill(picottl::colors::kWhite);
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
