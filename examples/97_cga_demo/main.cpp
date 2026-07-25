// PicoTTL - Demonstration 97: IBM Color Graphics Adapter
//
// OFFICIAL SHOWCASE DEMONSTRATION. Unlike the numbered examples, which
// each validate one framework feature, this program presents the
// complete capabilities of one display standard in a polished,
// historically authentic way - software that could plausibly have
// shipped on an IBM demonstration disk. Its two siblings (96: MDA,
// 98: EGA) are deliberately INDEPENDENT programs: a CGA monitor cannot
// display EGA, and an MDA monitor cannot display either, so each
// demonstration is a self-contained application dedicated to the one
// monitor its audience owns.
//
// Program flow:
//   1. Animated introduction (RGBI color sweep, banner lettering from
//      the CGA 8x8 character generator with drop shadow).
//   2. The sixteen RGBI colors  - the full gamut, labeled.
//   3. The 320x200 palettes     - the four historical palettes as data.
//   4. Geometric primitives     - outlines, fills, aspect-corrected
//      circles, line fans.
//   5. Ordered dithering        - smooth ramps and "new" colors.
//   6. A composed sunset        - text over graphics, scene composition.
//   7. Sprite animation         - software sprites in motion.
//   8. Color transitions        - the RGBI gamut in motion.
//   9. Technical information    - classic diagnostics-style readout.
//  10. Closing screen (remains indefinitely).
//
// The demonstration emphasises both the strengths and the limitations
// of CGA: everything on screen uses exactly the sixteen RGBI levels of
// the original hardware.
//
// ADAPTER: following PicoTTL's canonical-connector philosophy, this
// demonstration drives the CGA monitor with EgaDisplayDevice in its
// CGA-compatible 640x200 mode - exactly how IBM's EGA card served CGA
// monitors. CGA color numbers travel as EGA 200-line pixel values
// (ega::kCga200Palette), placing intensity on Secondary Green = DE-9
// pin 6. The standalone CGA adapter remains available as
// CgaDisplayDevice (see Examples 25-33).
//
// Wiring - the PicoTTL reference connector (470 ohm series resistors on
// HSYNC and VSYNC only; connect all other monitor signal wires directly.
// monitor signal wires directly. GND goes to DE-9 pin 1 only):
//   GPIO 2  -> HSYNC     (DE-9 pin 8)
//   GPIO 3  -> VSYNC     (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 16 -> BLUE      (DE-9 pin 5)
//   GPIO 17 -> GREEN     (DE-9 pin 4)
//   GPIO 18 -> RED       (DE-9 pin 3)
//   GPIO 19 -> DE-9 pin 7 (reserved on CGA monitors; stays low)
//   GPIO 20 -> INTENSITY (DE-9 pin 6)
//   GPIO 21 -> DE-9 pin 2 (GND inside CGA monitors; stays low)
//   GND     -> GND       (DE-9 pin 1)
//
// The onboard LED blinks slowly while the demonstration runs, fast on
// error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/fonts/IbmCga8x8.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

#include "DemoDraw.hpp"
#include "DemoText.hpp"
#include "Intro.hpp"
#include "Scenes.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmCga640;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer8::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

} // namespace

int main() {
    // 142.8 MHz: integer divider 10 -> 14.28 MHz pixel clock (-0.27%),
    // V = 59.76 Hz - the recommended CGA clock (see Example 29).
    set_sys_clock_khz(142'800, true);

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
    config.secondaryGreenPin = 20; // = CGA intensity in 200-line modes.
    config.secondaryRedPin = 21;
    config.framebuffer = &framebuffer;

    picottl::rp2350::EgaDisplayDevice device(config);
    picottl::Display display(device);

    const bool ok = display.begin(kMode);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    picottl::Graphics& g = display.graphics();
    picottl::Text text(g, picottl::fonts::kIbmCga8x8);

    namespace demo = picottl::demo;
    using namespace cga_demo;

    if (ok) {
        runIntro(g, text);

        sceneRgbi(g, text);
        demo::wipeDown(g, cga::kBlack, 8, 10);

        scenePalettes(g, text);
        demo::wipeDown(g, cga::kBlack, 8, 10);

        scenePrimitives(g, text);
        demo::fizzleFade(g, cga::kBlack, 1000);

        sceneDither(g, text);
        demo::wipeDown(g, cga::kBlack, 8, 10);

        sceneSunset(g, text);
        demo::fizzleFade(g, cga::kBlack, 1200);

        sceneSprites(g, text);
        demo::wipeDown(g, cga::kBlack, 8, 10);

        sceneColorCycle(g, text);
        demo::wipeDown(g, cga::kBlack, 8, 10);

        // Technical information - every value from the public API.
        demo::TechInfo info;
        info.standard = "IBM Color Graphics Adapter";
        info.videoMode = "IbmCga640 (640 x 200 at 60 Hz)";
        info.width = display.width();
        info.height = display.height();
        info.refreshMilliHz = display.refreshRateMilliHz();
        info.pixelFormat = "Packed8 (EGA scanout, CGA colors on the wire)";
        info.framebufferBytes = sizeof gFramebufferStorage;
        info.font = "IBM CGA 8x8";
        info.columns = text.columns();
        info.rows = text.rows();
        info.backend = "RP2350 EGA family, CGA-compatible mode";
        demo::drawTechInfoScreen(text, info, kScreen);
        sleep_ms(9000);
        demo::wipeDown(g, cga::kBlack, 8, 10);

        demo::drawEndingScreen(text, "IBM Color Graphics Adapter", kScreen);
    }

    while (true) {
#ifdef PICO_DEFAULT_LED_PIN
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(ok ? 900 : 100);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
        sleep_ms(ok ? 900 : 100);
#else
        sleep_ms(1000);
#endif
    }
}
