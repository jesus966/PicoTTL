// PicoTTL - Example 23: terminal
//
// Validates: the Terminal layer end to end - the character cell buffer
//   as source of truth, blinking text (per-cell attribute), the
//   non-destructive overlay cursor (block and underline styles,
//   configurable blink), automatic scrolling that preserves cell
//   attributes, and injected timing via Terminal::update(nowMs).
//   Also demonstrates Graphics::invertRect() as a general raster
//   primitive (a "selection" highlight).
//
// Expected output: a header with a title; a line blinking roughly once
//   per second (glyphs vanish, background stays); an underlined line; a
//   line highlighted by inversion (bright background band); then a
//   growing log of numbered lines that starts scrolling when it reaches
//   the bottom. A block cursor blinks at the end of the log output,
//   switching to underline style and back every ~6 seconds. Blinking
//   and the cursor never corrupt the underlying text: whenever they
//   hide, the original content reappears intact. The header scrolls
//   away naturally once the log fills the screen - with its attributes
//   (including blink) preserved while it moves up.
//
// Regressions detected: cell-buffer desynchronization (wrong characters
//   reappearing after cursor/blink hide), overlay corruption (cursor
//   leaving inverse-video or underline residue behind), blink phase
//   affecting non-blink cells, attribute leakage between writes, scroll
//   losing cell attributes, timing regressions (blink rates wrong).
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

// Cell storage sized from the mode and the 9x14 MDA font cell - the
// Terminal itself derives its geometry at runtime and assumes nothing.
constexpr std::int32_t kColumns = picottl::modeWidth(kMode) / 9;
constexpr std::int32_t kRows = picottl::modeHeight(kMode) / 14;
picottl::TerminalCell gCells[picottl::Terminal::cellsFor(kColumns, kRows)];

// MDA backend color indices (bit 0 = VIDEO, bit 1 = INTENSITY).
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
    picottl::Terminal terminal(text, gCells,
                               sizeof gCells / sizeof gCells[0]);

    terminal.clear();
    terminal.setAttributes({kBright, kBlack, false, false});
    terminal.print("  PicoTTL Terminal Demo\n\n");
    terminal.setAttributes({kNormal, kBlack, false, true});
    terminal.print("  This line BLINKS like a classic IBM attribute\n");
    terminal.setAttributes({kNormal, kBlack, true, false});
    terminal.print("  This line is underlined\n");
    terminal.setAttributes({kNormal, kBlack, false, false});
    terminal.print("  This line gets an invertRect selection band\n\n");

    // Graphics::invertRect() as a general raster primitive: a raw
    // "selection" highlight over the line above. Note it acts on pixels,
    // not cells - it will be overwritten naturally when those cells are
    // re-rendered (e.g. once scrolling reaches them).
    display.graphics().invertRect(2 * 9, 5 * 14, 46 * 9, 14);

    terminal.setCursorStyle(picottl::CursorStyle::Block);

    const bool ok = display.begin(kMode);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    long lineNumber = 0;
    std::uint32_t lastLineMs = 0;
    std::uint32_t lastStyleMs = 0;
    bool blockCursor = true;
    while (true) {
        const std::uint32_t nowMs = to_ms_since_boot(get_absolute_time());
        terminal.update(nowMs);

        if (nowMs - lastLineMs >= 900) { // Log output -> auto-scroll.
            lastLineMs = nowMs;
            terminal.printf("  log %ld: terminal output with automatic scrolling\n",
                            lineNumber++);
        }
        if (nowMs - lastStyleMs >= 6000) { // Alternate cursor styles.
            lastStyleMs = nowMs;
            blockCursor = !blockCursor;
            terminal.setCursorStyle(blockCursor
                                        ? picottl::CursorStyle::Block
                                        : picottl::CursorStyle::Underline);
        }
        sleep_ms(ok ? 20 : 100);
    }
}
