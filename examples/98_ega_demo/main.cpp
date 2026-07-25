// PicoTTL - Demonstration 98: IBM Enhanced Graphics Adapter
//
// OFFICIAL SHOWCASE DEMONSTRATION. Unlike the numbered examples, which
// each validate one framework feature, this program presents the
// complete capabilities of one display standard in a polished,
// historically authentic way - software that could plausibly have
// shipped on an IBM demonstration disk. Its two siblings (96: MDA,
// 97: CGA) are deliberately INDEPENDENT programs: each demonstration
// is a self-contained application dedicated to the one monitor its
// audience owns.
//
// Program flow:
//   1. Animated introduction (64-color sweep, banner lettering from
//      the EGA 8x14 character generator).
//   2. Sixty-four colors      - the full gamut, hex-labeled.
//   3. Sixteen from sixty-four - the power-on palette, then an amber
//      palette no CGA could show.
//   4. Gun levels and gradients - four levels per gun, dithered smooth.
//   5. Shaded spheres         - per-pixel shading through the gamut.
//   6. A composed vista       - the CGA demo's sunset, revisited with
//      sixty-four colors (the leap, made visible).
//   7. Smooth animation       - 280 scanlines scrolled per frame by
//      framebuffer row copies.
//   8. The EGA leap           - mixed text and graphics chart.
//   9. Technical information  - classic diagnostics-style readout.
//  10. Closing screen (remains indefinitely).
//
// Wiring (use 470 ohm series resistors on HSYNC and VSYNC; connect all
// other monitor signal wires directly):
//   GPIO 2  -> HSYNC           (DE-9 pin 8)
//   GPIO 3  -> VSYNC           (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 16 -> PRIMARY BLUE    (DE-9 pin 5)
//   GPIO 17 -> PRIMARY GREEN   (DE-9 pin 4)
//   GPIO 18 -> PRIMARY RED     (DE-9 pin 3)
//   GPIO 19 -> SECONDARY BLUE  (DE-9 pin 7)
//   GPIO 20 -> SECONDARY GREEN (DE-9 pin 6)
//   GPIO 21 -> SECONDARY RED   (DE-9 pin 2)
//   GND     -> GND             (DE-9 pin 1 ONLY - pin 2 is a signal!)
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
#include "picottl/ega/Colors.hpp"
#include "picottl/fonts/IbmEga8x14.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

#include "DemoDraw.hpp"
#include "DemoText.hpp"
#include "Intro.hpp"
#include "Scenes.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmEga350;

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

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    picottl::Graphics& g = display.graphics();
    picottl::Text text(g, picottl::fonts::kIbmEga8x14);

    namespace demo = picottl::demo;
    namespace ega = picottl::ega;
    using namespace ega_demo;

    if (ok) {
        runIntro(g, text);

        scene64Colors(g, text);
        demo::wipeDown(g, ega::kBlack, 14, 8);

        scenePalettes(g, text);
        demo::wipeDown(g, ega::kBlack, 14, 8);

        sceneGradients(g, text);
        demo::fizzleFade(g, ega::kBlack, 1200);

        sceneSpheres(g, text);
        demo::wipeDown(g, ega::kBlack, 14, 8);

        sceneVista(g, text);
        demo::fizzleFade(g, ega::kBlack, 1400);

        sceneWaterfall(g, text);
        demo::wipeDown(g, ega::kBlack, 14, 8);

        sceneChart(g, text);
        demo::wipeDown(g, ega::kBlack, 14, 8);

        // Technical information - every value from the public API.
        demo::TechInfo info;
        info.standard = "IBM Enhanced Graphics Adapter";
        info.videoMode = "IbmEga350 (640 x 350 at 60 Hz)";
        info.width = display.width();
        info.height = display.height();
        info.refreshMilliHz = display.refreshRateMilliHz();
        info.pixelFormat = "Packed8 scanout (low 6 bits drive rgbRGB)";
        info.framebufferBytes = sizeof gFramebufferStorage;
        info.font = "IBM EGA 8x14";
        info.columns = text.columns();
        info.rows = text.rows();
        info.backend = "RP2350 EGA family (PIO + DMA)";
        demo::drawTechInfoScreen(text, info, kScreen);
        sleep_ms(9000);
        demo::wipeDown(g, ega::kBlack, 14, 8);

        demo::drawEndingScreen(text, "IBM Enhanced Graphics Adapter",
                               kScreen);
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
