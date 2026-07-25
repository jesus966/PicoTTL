// PicoTTL - Example 22: text attributes
//
// Validates: the MDA text attribute set on the validated 2 bpp pixel
//   path - bright text via Text::setColors() (Color{3} = VIDEO +
//   INTENSITY on the MDA backend), underline via Text::setUnderline()
//   (drawn on the font's underline row 12, like the original hardware),
//   inverse video, and attribute state save/restore across lines.
//
// Expected output, from the top-left, one attribute demo per line:
//     Normal text
//     Bright text                (visibly brighter)
//     Underlined text            (continuous underline on glyph row 12)
//     Bright underlined text     (both)
//     Inverse video text         (black glyphs on a lit row of cells)
//     Normal again               (state correctly restored)
//   Underlines must be continuous under the whole text, one pixel high,
//   and bright exactly when the text is bright.
//
// Regressions detected: attribute state leaking between lines, underline
//   drawn on the wrong row or in the wrong color, bright rendering
//   affecting the background, inverse video not painting full cells,
//   color-index mapping errors in the packed framebuffer.
//
// Wiring (direct GPIO connection to the DE-9 works; if using series
// resistors keep them small and MATCHED, <= ~100 ohm):
//   GPIO 2  -> HSYNC     (DE-9 pin 8)
//   GPIO 3  -> VSYNC     (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 19 -> VIDEO     (DE-9 pin 7)
//   GPIO 20 -> INTENSITY (DE-9 pin 6)
//   GND     -> GND       (DE-9 pin 1 only on the shared reference connector)
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmMda;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer2::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

// Color indices as interpreted by the MDA backend (bit 0 = VIDEO,
// bit 1 = INTENSITY). Abstract indices: other backends map differently.
constexpr picottl::Color kBlack{0};
constexpr picottl::Color kNormal{1};
constexpr picottl::Color kBright{3};

} // namespace

int main() {
    set_sys_clock_khz(130'000, true);

    picottl::PackedFramebuffer2 framebuffer(
        gFramebufferStorage, sizeof gFramebufferStorage,
        picottl::modeWidth(kMode), picottl::modeHeight(kMode));

    picottl::rp2350::MdaDisplayDevice::Config config;
    config.hsyncPin = 2;      // VSYNC on GPIO 3; GPIO 4 internal, unwired.
    config.pixelPins[0] = 19; // VIDEO
    config.pixelPins[1] = 20; // INTENSITY
    config.framebuffer = &framebuffer;

    picottl::rp2350::MdaDisplayDevice device(config);
    picottl::Display display(device);

    picottl::Text text(display.graphics(), picottl::fonts::kIbmMda9x14);
    text.clear();
    text.setCursor(0, 1);

    text.print("  Normal text\n\n");

    text.setColors(kBright, kBlack);
    text.print("  Bright text\n\n");

    text.setColors(kNormal, kBlack);
    text.setUnderline(true);
    text.print("  Underlined text\n\n");

    text.setColors(kBright, kBlack);
    text.print("  Bright underlined text\n\n");

    text.setUnderline(false);
    text.setColors(kBlack, kNormal);
    text.print("  Inverse video text  \n\n");

    text.setColors(kNormal, kBlack);
    text.print("  Normal again\n");

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
