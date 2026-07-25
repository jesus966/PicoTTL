// PicoTTL - Example 44: IBM code pages (CP437 vs CP850)
//
// Validates: the code page architecture on the validated MDA text
//   path - Font codePage metadata, the kIbmMda9x14Family font family,
//   Text::setCodePage() (renderer-level switching, already-drawn
//   pixels untouched) and Terminal::setCodePage() (whole-screen
//   reinterpretation from the byte-valued cell buffer - the authentic
//   DOS MODE CON CP SELECT semantics).
//
// The example alternates two phases forever:
//
//   Phase A (16 s) - static comparison via Text. The SAME byte strings
//   are printed twice: first under CP437, then - after
//   text.setCodePage(Cp850) - under CP850. Because Text has no cell
//   buffer, both renderings coexist on screen:
//     - the lowercase accents line (a-acute, e-acute, ..., n-tilde,
//       c-cedilla, u-umlaut, inverted ? and !) is IDENTICAL in both
//       sections: those characters share positions across the pages;
//     - the uppercase accents line reads box-drawing fragments and
//       Greek letters under CP437 but A/E/I/O/U-acute, A-grave,
//       A/E-circumflex, A-tilde under CP850;
//     - the signs line reads cent/yen/peseta/math under CP437 but
//       o-slash, multiplication, (c)/(R), eth, thorn, section,
//       pilcrow, 3/4 under CP850.
//
//   Phase B (4 cycles x 4 s) - dynamic switching via Terminal. A
//   DOS-flavored session (dir + type of a Western-European text file)
//   is written ONCE; then setCodePage() alternates 437/850. The whole
//   screen must reinterpret INSTANTLY on each switch - cells store
//   bytes, exactly like the hardware character generator did - with a
//   blinking block cursor proving the terminal stays live.
//
// Regressions detected: family lookup or metadata errors, CP850
//   dataset corruption, repaint-on-switch failures, geometry drift
//   after a font swap (the family invariant), fallback rendering.
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
#include "picottl/Terminal.hpp"
#include "picottl/fonts/IbmMda9x14Family.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmMda;

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer2::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

picottl::TerminalCell gCells[picottl::Terminal::cellsFor(80, 25)];

// Color indices as interpreted by the MDA backend (bit 0 = VIDEO,
// bit 1 = INTENSITY). Abstract indices: other backends map differently.
constexpr picottl::Color kBlack{0};
constexpr picottl::Color kNormal{1};
constexpr picottl::Color kBright{3};

// The demonstration strings, written as EXPLICIT BYTES: what they
// display depends solely on the active code page.
//
// Identical in both pages (shared positions):
//   a-acute e-acute i-acute o-acute u-acute, n-tilde, c-cedilla,
//   u-umlaut, inverted question/exclamation marks.
constexpr const char* kLineShared =
    "a\xA0 e\x82 i\xA1 o\xA2 u\xA3   \xA4   \x87   \x81   \xA8   \xAD";

// CP437: box fragments and Greek. CP850: A-acute E-acute I-acute
// O-acute U-acute, A-tilde, A-grave, E-circumflex, E-grave.
constexpr const char* kLineUpper =
    "\xB5 \x90 \xD6 \xE0 \xE9   \xC7   \xB7 \xD2 \xD4";

// CP437: cent yen peseta reversed-not frame-pieces tau Phi <= integral
// halves left-half-block. CP850: o-slash O-slash multiplication (R)
// (c) eth Eth thorn Thorn section pilcrow three-quarters broken-bar.
constexpr const char* kLineSigns =
    "\x9B \x9D \x9E \xA9 \xB8 \xD0 \xD1 \xE7 \xE8 \xF5 \xF4 \xF3 \xDD";

// "!Manana sera otro dia!  Ca va tres bien.  ACAO ARBOL ETRE" with the
// proper accents - fully correct only under CP850 (the uppercase
// accented letters render as frame fragments under CP437).
constexpr const char* kLineSentence =
    "\xADMa\xA4" "ana ser\xA0 otro d\xA1" "a!  \x80" "a va tr\x8A"
    "s bien.  A\x87\xC6O  \xB5RBOL  \xD2TRE";

void printSampleBlock(picottl::Text& text) {
    text.setColors(kNormal, kBlack);
    text.printf("    minuscules  %s\n", kLineShared);
    text.printf("    majuscules  %s\n", kLineUpper);
    text.printf("    signs       %s\n", kLineSigns);
    text.printf("    sentence    %s\n", kLineSentence);
}

void drawStaticComparison(picottl::Text& text) {
    // Start from the historical default regardless of previous phase.
    text.setCodePage(picottl::CodePage::Cp437);
    text.setColors(kNormal, kBlack);
    text.clear();

    text.setColors(kBright, kBlack);
    text.print("\n  PicoTTL - IBM code pages: the same bytes under CP437"
               " and CP850\n\n");

    text.setColors(kBright, kBlack);
    text.print("  Code page 437 (IBM PC, 1981)\n\n");
    printSampleBlock(text);

    text.setCodePage(picottl::CodePage::Cp850);
    text.setColors(kBright, kBlack);
    text.print("\n  Code page 850 (DOS Multilingual/Latin-1, DOS 3.3+)"
               "\n\n");
    printSampleBlock(text);

    text.setColors(kNormal, kBlack);
    text.print("\n  The byte strings above are identical. Text::setCodePage()"
               " only affects\n  subsequent rendering, so both interpretations"
               " coexist; the lowercase line\n  matches because CP437 and"
               " CP850 share those positions.");
}

void printTerminalSession(picottl::Terminal& terminal) {
    terminal.clear();
    picottl::CellAttributes attrs;
    attrs.foreground = kBright;
    terminal.setAttributes(attrs);
    terminal.print("  PicoTTL - Terminal::setCodePage(): MODE CON CP SELECT"
                   " semantics\n\n");
    attrs.foreground = kNormal;
    terminal.setAttributes(attrs);
    terminal.print("C:\\>type europa.txt\n");
    terminal.printf("%s\n", kLineSentence);
    terminal.printf("%s\n", kLineUpper);
    terminal.printf("%s\n\n", kLineSigns);
    terminal.print("C:\\>rem Cells store BYTES: switching the code page\n"
                   "C:\\>rem repaints and the WHOLE screen reinterprets.\n\n");
}

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

    // Managed (family) mode: both datasets linked, CP437 active.
    picottl::Text text(display.graphics(), picottl::fonts::kIbmMda9x14Family);

    const bool ok = display.begin(kMode);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    while (true) {
        // ------------------------------------------------- Phase A --
        drawStaticComparison(text);
        sleep_ms(16'000);

        // ------------------------------------------------- Phase B --
        text.setCodePage(picottl::CodePage::Cp437);
        picottl::Terminal terminal(text, gCells,
                                   sizeof gCells / sizeof gCells[0]);
        terminal.setCursorStyle(picottl::CursorStyle::Block);
        printTerminalSession(terminal);

        for (int cycle = 0; cycle < 4; ++cycle) {
            const bool useCp850 = (cycle % 2) == 0;
            terminal.setCodePage(useCp850 ? picottl::CodePage::Cp850
                                          : picottl::CodePage::Cp437);
            terminal.setCursor(0, 23);
            terminal.clearLine();
            terminal.printf("C:\\>chcp %s", useCp850 ? "850" : "437");

            const std::uint32_t phaseEnd =
                to_ms_since_boot(get_absolute_time()) + 4'000;
            while (to_ms_since_boot(get_absolute_time()) < phaseEnd) {
                terminal.update(to_ms_since_boot(get_absolute_time()));
                sleep_ms(20);
            }
        }
    }
}
