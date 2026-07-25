// PicoTTL - Example 42: ANSI serial terminal on EGA
//
// Examples 24 (MDA) and 33 (CGA) ported to the EGA backend - the
// three-way API-stability proof of the framework. Diff this file
// against either sibling: everything that changes is the
// application's own declarations (device type + pins, framebuffer
// format, font, palette, clock); every AnsiTerminal / Terminal /
// Text / Display call is character-for-character identical across all
// three backends. The AnsiPalette here is the IBM EGA power-on
// palette, so SGR colors render exactly as an EGA PC did - including
// the real brown (0x14) for SGR 33.
//
//   USB CDC -> AnsiTerminal -> Terminal -> Text -> Graphics -> framebuffer
//
// Try it with PuTTY / Tera Term / minicom / picocom / screen at any
// baud rate (USB CDC ignores it).
//
// Expected: a banner appears on the monitor at power-up; every byte
// typed in the attached terminal program is echoed and rendered, with
// SGR colors mapping to the EGA power-on palette (SGR 33 = brown 0x14).
//
// A failure here (with Examples 40-41 passing) indicates a regression
// in AnsiTerminal parsing/state or in the USB CDC polling loop, not in
// the display path.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// lines - they also damp induced glitches on the sync inputs):
//   GPIO 2  -> HSYNC           (DE-9 pin 8)
//   GPIO 3  -> VSYNC           (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 16 -> PRIMARY BLUE    (DE-9 pin 5)
//   GPIO 17 -> PRIMARY GREEN   (DE-9 pin 4)
//   GPIO 18 -> PRIMARY RED     (DE-9 pin 3)
//   GPIO 19 -> SECONDARY BLUE  (DE-9 pin 7)
//   GPIO 20 -> SECONDARY GREEN (DE-9 pin 6)
//   GPIO 21 -> SECONDARY RED   (DE-9 pin 2)
//   GND     -> GND             (DE-9 pin 1)
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/AnsiTerminal.hpp"
#include "picottl/PicoTTL.hpp"
#include "picottl/ega/Colors.hpp"
#include "picottl/fonts/IbmEga8x14.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmEga350;

/// Echo received characters back over the serial link, so a directly
/// attached terminal program shows what is typed. Disable for use as a
/// pure remote display.
constexpr bool kLocalEcho = true;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer8::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

picottl::TerminalCell gCells[picottl::Terminal::cellsFor(
    picottl::modeWidth(kMode) / 8, picottl::modeHeight(kMode) / 14)];

/// ANSI colors on EGA: the IBM power-on palette. ANSI index i (SGR
/// 30-37/90-97, bold = +8) maps to the same EGA value the hardware
/// initialized its palette register i with - authentic period colors,
/// brown included.
picottl::AnsiPalette makeEgaPalette() {
    picottl::AnsiPalette palette;
    for (int i = 0; i < 16; ++i) {
        palette.foreground[i] = picottl::ega::kDefaultPalette[i];
        palette.background[i] = picottl::ega::kDefaultPalette[i];
    }
    palette.defaultForeground = picottl::ega::kLightGray;
    palette.defaultBackground = picottl::ega::kBlack;
    return palette;
}

} // namespace

int main() {
    // 130 MHz: the EGA 350-line raster shares MDA's pixel crystal
    // (integer divider 8, -0.043% - see Example 34).
    set_sys_clock_khz(130'000, true);
    stdio_init_all(); // USB CDC (see CMakeLists: USB on, UART off)

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
    picottl::Text text(display.graphics(), picottl::fonts::kIbmEga8x14);
    picottl::Terminal terminal(text, gCells, sizeof gCells / sizeof gCells[0]);
    picottl::AnsiTerminal ansi(terminal, makeEgaPalette());

    const bool ok = display.begin(kMode);

    terminal.clear();
    terminal.setCursorStyle(picottl::CursorStyle::Block);
    terminal.printf("PicoTTL ANSI serial terminal  v%d.%d.%d (EGA)\n",
                    PICOTTL_VERSION_MAJOR, PICOTTL_VERSION_MINOR,
                    PICOTTL_VERSION_PATCH);
    terminal.printf("%ldx%ld cells, default palette over USB CDC%s\n\n",
                    static_cast<long>(terminal.columns()),
                    static_cast<long>(terminal.rows()),
                    kLocalEcho ? ", local echo on" : "");

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    while (true) {
        terminal.update(to_ms_since_boot(get_absolute_time()));

        // Drain pending serial input (bounded per iteration so blink
        // timing stays responsive under sustained input).
        for (int budget = 512; budget > 0; --budget) {
            const int received = getchar_timeout_us(0);
            if (received < 0) {
                break;
            }
            // With local echo this example is the line editor, so
            // Backspace (BS, and DEL as sent by PuTTY by default) must
            // erase. A plain BS only moves the cursor; the destructive
            // "BS SP BS" echo normally comes from the remote host.
            if (kLocalEcho &&
                (received == 0x08 || received == 0x7F)) {
                static const std::uint8_t kRubout[] = {0x08, ' ', 0x08};
                for (const std::uint8_t byte : kRubout) {
                    putchar_raw(byte); // mirror the erase in PuTTY too
                    ansi.processByte(byte);
                }
                continue;
            }
            if (kLocalEcho) {
                putchar_raw(received);
            }
            ansi.processByte(static_cast<std::uint8_t>(received));
        }
        sleep_us(500);
    }
}
