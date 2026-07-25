// PicoTTL - Example 33: ANSI serial terminal on CGA
//
// Example 24 ported to the CGA backend - THE API-stability proof of
// the framework. Diff this file against 24_serial_terminal/main.cpp:
// everything that changes is the application's own declarations
// (device type + pins, framebuffer format, font, palette, clock);
// every AnsiTerminal / Terminal / Text / Display call is character-
// for-character identical. And the payoff of the AnsiPalette design:
// on CGA the palette is simply the identity - all 16 ANSI/SGR colors
// map 1:1 to the RGBI colors, so `ls --color`, htop and friends
// render exactly as they did on a real CGA PC.
//
//   USB CDC -> AnsiTerminal -> Terminal -> Text -> Graphics -> framebuffer
//
// Try it with PuTTY / Tera Term / minicom / picocom / screen at any
// baud rate (USB CDC ignores it).
//
// Expected: a banner appears on the monitor at power-up; every byte
// typed in the attached terminal program is echoed and rendered, with
// all 16 SGR colors mapping 1:1 to RGBI.
//
// A failure here (with Examples 31-32 passing) indicates a regression
// in AnsiTerminal parsing/state or in the USB CDC polling loop, not in
// the display path.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// lines - they also damp induced glitches on the sync inputs):
//   GPIO 2  -> HSYNC     (DE-9 pin 8)
//   GPIO 3  -> VSYNC     (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 16 -> BLUE      (DE-9 pin 5)
//   GPIO 17 -> GREEN     (DE-9 pin 4)
//   GPIO 18 -> RED       (DE-9 pin 3)
//   GPIO 19 -> INTENSITY (DE-9 pin 6)
//   GND     -> GND       (DE-9 pin 1 only)
//
// NOTE: this is the AUTHENTIC STANDALONE CGA ADAPTER pinout, not the
// PicoTTL reference connector (EGA superset, where DE-9 pin 6 is fed
// from GPIO 20). On the reference connector, drive CGA monitors with
// EgaDisplayDevice's CGA modes instead (see Example 38 and
// docs/architecture.md).
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/AnsiTerminal.hpp"
#include "picottl/PicoTTL.hpp"
#include "picottl/fonts/IbmCga8x8.hpp"
#include "picottl/rp2350/CgaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmCga640;

/// Echo received characters back over the serial link, so a directly
/// attached terminal program shows what is typed. Disable for use as a
/// pure remote display.
constexpr bool kLocalEcho = true;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer4::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

picottl::TerminalCell gCells[picottl::Terminal::cellsFor(
    picottl::modeWidth(kMode) / 8, picottl::modeHeight(kMode) / 8)];

/// ANSI colors on CGA: the identity mapping. The pixel value IS the
/// IBM color number, so ANSI index i (SGR 30-37/90-97, bold = +8) is
/// simply Color{i} - all 16 foregrounds and backgrounds, faithfully.
picottl::AnsiPalette makeCgaPalette() {
    picottl::AnsiPalette palette;
    for (int i = 0; i < 16; ++i) {
        palette.foreground[i] = picottl::Color{static_cast<std::uint8_t>(i)};
        palette.background[i] = picottl::Color{static_cast<std::uint8_t>(i)};
    }
    palette.defaultForeground = picottl::Color{7}; // light gray
    palette.defaultBackground = picottl::Color{0}; // black
    return palette;
}

} // namespace

int main() {
    // 142.8 MHz: jitter-free integer clock dividers for both CGA modes,
    // vertical ~59.76 Hz (slightly BELOW nominal - see Example 25).
    set_sys_clock_khz(142'800, true);
    stdio_init_all(); // USB CDC (see CMakeLists: USB on, UART off)

    picottl::PackedFramebuffer4 framebuffer(
        gFramebufferStorage, sizeof gFramebufferStorage,
        picottl::modeWidth(kMode), picottl::modeHeight(kMode));

    picottl::rp2350::CgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3; // GPIO 4 internal handshake, leave unwired.
    config.bluePin = 16;
    config.greenPin = 17;
    config.redPin = 18;
    config.intensityPin = 19;
    config.framebuffer = &framebuffer;

    picottl::rp2350::CgaDisplayDevice device(config);
    picottl::Display display(device);
    picottl::Text text(display.graphics(), picottl::fonts::kIbmCga8x8);
    picottl::Terminal terminal(text, gCells, sizeof gCells / sizeof gCells[0]);
    picottl::AnsiTerminal ansi(terminal, makeCgaPalette());

    const bool ok = display.begin(kMode);

    terminal.clear();
    terminal.setCursorStyle(picottl::CursorStyle::Block);
    terminal.printf("PicoTTL ANSI serial terminal  v%d.%d.%d (CGA)\n",
                    PICOTTL_VERSION_MAJOR, PICOTTL_VERSION_MINOR,
                    PICOTTL_VERSION_PATCH);
    terminal.printf("%ldx%ld cells, 16 colors over USB CDC%s\n\n",
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
