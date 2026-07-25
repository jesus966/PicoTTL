// PicoTTL - Demonstration 96: IBM Monochrome Display Adapter
//
// OFFICIAL SHOWCASE DEMONSTRATION. Unlike the numbered examples, which
// each validate one framework feature, this program presents the
// complete capabilities of one display standard in a polished,
// historically authentic way - software that could plausibly have
// shipped on an IBM demonstration disk. Its three siblings (97: CGA,
// 98: EGA) are deliberately INDEPENDENT programs: an MDA monitor
// cannot display CGA or EGA, so each demonstration is a self-contained
// application dedicated to the one monitor its audience owns.
//
// Program flow:
//   1. Animated introduction (blinking cursor, typed BIOS-style
//      messages, banner lettering from the 9x14 character generator).
//   2. Text attributes    - normal, bright, underline, reverse, blink.
//   3. Windows and menus  - the DOS-era text user interface.
//   4. Character set      - all 256 CP437 glyphs.
//   5. Full-raster art    - line graphics on all 720x350 pixels.
//   6. Text-mode animation - marquee and bouncing block.
//   7. Terminal session   - the PicoTTL Terminal layer, simulated DOS.
//   8. Technical information - classic diagnostics-style readout.
//   9. Closing screen (remains indefinitely).
//
// Scene transitions use period techniques: top-down wipes and an
// LFSR pixel dissolve ("fizzle fade").
//
// Serves four purposes: demonstrates the capabilities of the IBM MDA
// standard; provides a production-quality reference for structuring a
// complete PicoTTL application; acts as a visual regression test for
// future PicoTTL releases; and celebrates the original IBM display
// hardware.
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
// The onboard LED blinks slowly while the demonstration runs, fast on
// error.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

#include "DemoDraw.hpp"
#include "DemoText.hpp"
#include "Intro.hpp"
#include "Scenes.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmMda;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer2::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

} // namespace

int main() {
    // 130 MHz: integer divider 8 -> 16.25 MHz pixel clock (-0.043%).
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

    const bool ok = display.begin(kMode);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    picottl::Graphics& g = display.graphics();
    picottl::Text text(g, picottl::fonts::kIbmMda9x14);

    namespace demo = picottl::demo;
    using namespace mda_demo;

    if (ok) {
        runIntro(g, text);

        sceneTextAttributes(g, text);
        demo::wipeDown(g, kBlack, 14, 10);

        sceneWindows(g, text);
        demo::wipeDown(g, kBlack, 14, 10);

        sceneCharacterSet(g, text);
        demo::fizzleFade(g, kBlack, 1100);

        sceneLineArt(g, text);
        demo::wipeDown(g, kBlack, 14, 10);

        sceneAnimation(g, text);
        demo::wipeDown(g, kBlack, 14, 10);

        sceneTerminal(g, text);
        demo::fizzleFade(g, kBlack, 1100);

        // Technical information - every value from the public API.
        demo::TechInfo info;
        info.standard = "IBM Monochrome Display Adapter";
        info.videoMode = "IbmMda (720 x 350 at 50 Hz)";
        info.width = display.width();
        info.height = display.height();
        info.refreshMilliHz = display.refreshRateMilliHz();
        info.pixelFormat = "Packed2 (2 bits per pixel: VIDEO + INTENSITY)";
        info.framebufferBytes = sizeof gFramebufferStorage;
        info.font = "IBM MDA 9x14";
        info.columns = text.columns();
        info.rows = text.rows();
        info.backend = "RP2350 MDA family (PIO + DMA)";
        demo::drawTechInfoScreen(text, info, kScreen);
        sleep_ms(9000);
        demo::wipeDown(g, kBlack, 14, 10);

        demo::drawEndingScreen(text, "IBM Monochrome Display Adapter",
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
