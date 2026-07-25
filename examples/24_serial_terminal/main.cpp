// PicoTTL - Example 24: ANSI serial terminal
//
// Turns PicoTTL into a real serial text terminal: bytes received over
// the USB CDC serial port are rendered on the MDA display through the
// layered text subsystem:
//
//   USB CDC -> AnsiTerminal -> Terminal -> Text -> Graphics -> framebuffer
//
// Terminal remains a generic terminal renderer (no ANSI knowledge);
// AnsiTerminal interprets escape sequences and translates them into
// Terminal operations; this example owns the USB interface (no polling
// logic leaks into rendering classes).
//
// Try it with PuTTY / Tera Term / minicom / picocom / screen at any
// baud rate (USB CDC ignores it). Output of programs like `ls --color`,
// `git log` or `htop` demonstrates the partial ANSI support; perfect
// VT100 compatibility is not the goal at this stage.
//
// Expected: a banner appears on the monitor at power-up; every byte
// typed in the attached terminal program is echoed and rendered, with
// cursor movement, colors-as-intensity, erase and scrolling honoring
// the ANSI sequences.
//
// A failure here (with Examples 22-23 passing) indicates a regression
// in AnsiTerminal parsing/state or in the USB CDC polling loop, not in
// the display path.
//
// Wiring (direct GPIO connection to the DE-9 works):
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

#include "picottl/AnsiTerminal.hpp"
#include "picottl/PicoTTL.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmMda;

/// Echo received characters back over the serial link, so a directly
/// attached terminal program shows what is typed. Disable for use as a
/// pure remote display.
constexpr bool kLocalEcho = true;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer2::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

picottl::TerminalCell gCells[picottl::Terminal::cellsFor(
    picottl::modeWidth(kMode) / 9, picottl::modeHeight(kMode) / 14)];

/// ANSI colors mapped to the MDA backend's capabilities: black stays
/// black, every dark color renders as normal video, every bright color
/// as VIDEO+INTENSITY. The parser itself never assumes MDA.
picottl::AnsiPalette makeMdaPalette() {
    picottl::AnsiPalette palette;
    for (int i = 0; i < 16; ++i) {
        palette.foreground[i] =
            i == 0 ? picottl::Color{0}
                   : (i < 8 ? picottl::Color{1} : picottl::Color{3});
        palette.background[i] =
            i == 0 ? picottl::Color{0} : picottl::Color{1};
    }
    palette.defaultForeground = picottl::Color{1};
    palette.defaultBackground = picottl::Color{0};
    return palette;
}

} // namespace

int main() {
    set_sys_clock_khz(130'000, true);
    stdio_init_all(); // USB CDC (see CMakeLists: USB on, UART off)

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
    picottl::Terminal terminal(text, gCells, sizeof gCells / sizeof gCells[0]);
    picottl::AnsiTerminal ansi(terminal, makeMdaPalette());

    const bool ok = display.begin(kMode);

    terminal.clear();
    terminal.setCursorStyle(picottl::CursorStyle::Block);
    terminal.printf("PicoTTL ANSI serial terminal  v%d.%d.%d\n",
                    PICOTTL_VERSION_MAJOR, PICOTTL_VERSION_MINOR,
                    PICOTTL_VERSION_PATCH);
    terminal.printf("%ldx%ld cells over USB CDC%s\n\n",
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
