// PicoTTL - Example 45: MDA phosphor rain
//
// Validates: sustained independent text-cell updates on the MDA Packed2
//   path, with normal and bright glyphs changing while PIO and DMA keep
//   scanout fully hardware-driven. The visual concept is inspired by
//   ybuzoku/MatrixScreensaver, an IBM PC/XT MDA text-mode program; this
//   implementation is original and uses PicoTTL's framebuffer API.
//
// Expected output: asynchronous columns of random IBM glyphs falling at
//   different speeds. Each stream has a bright leading character and a
//   normal-intensity tail. On an IBM 5151 or another medium/high-persistence
//   phosphor monitor, erased tail cells remain briefly visible and produce
//   a smooth third intensity level that is not stored in the framebuffer.
//
// Regressions detected: VIDEO/INTENSITY mapping errors (missing bright
//   heads), text-cell residue, unstable scanout during frequent framebuffer
//   writes, synchronized or repeating streams caused by RNG/state errors.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect pixel lines
// directly because the 5151 inputs are low-impedance summing networks):
//   GPIO 2  -> HSYNC     (DE-9 pin 8)
//   GPIO 3  -> VSYNC     (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 19 -> VIDEO     (DE-9 pin 7)
//   GPIO 20 -> INTENSITY (DE-9 pin 6)
//   GND     -> GND       (DE-9 pin 1 only on the shared reference connector)
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <array>
#include <cstddef>
#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

namespace {

constexpr auto kMode = picottl::VideoMode::IbmMda;
constexpr std::int32_t kColumns = 80;
constexpr std::int32_t kRows = 25;
constexpr std::uint32_t kFrameDelayMs = 35;

constexpr picottl::Color kBlack{0};
constexpr picottl::Color kNormal{1};
constexpr picottl::Color kBright{3};

constexpr char kGlyphs[] =
    "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
    "@#$%&*+-=<>[]{}|/\\:;!?"
    "\xB0\xB1\xB2\xB3\xB4\xB5\xB6\xB7\xB8\xB9\xBA\xBB\xBC\xBD\xBE\xBF";

struct Column {
    std::int16_t head = -1;
    std::uint8_t length = 8;
    std::uint8_t speed = 1;
    std::uint8_t phase = 0;
    std::uint8_t pause = 0;
};

alignas(std::uint32_t) std::uint8_t gFramebufferStorage
    [picottl::PackedFramebuffer2::bytesFor(picottl::modeWidth(kMode),
                                           picottl::modeHeight(kMode))];

std::array<Column, kColumns> gColumns;
std::array<std::array<unsigned char, kRows>, kColumns> gCells{};
std::uint32_t gRandomState = 0x5151'2026u;

std::uint32_t randomValue() {
    gRandomState ^= gRandomState << 13;
    gRandomState ^= gRandomState >> 17;
    gRandomState ^= gRandomState << 5;
    return gRandomState;
}

std::uint32_t randomBelow(std::uint32_t limit) {
    return randomValue() % limit;
}

unsigned char randomGlyph() {
    constexpr std::size_t kGlyphCount = sizeof kGlyphs - 1;
    return static_cast<unsigned char>(kGlyphs[randomBelow(kGlyphCount)]);
}

void drawCell(picottl::Text& text, std::int32_t column, std::int32_t row,
              unsigned char glyph, picottl::Color color) {
    if (row < 0 || row >= kRows) {
        return;
    }

    text.setColors(color, kBlack);
    text.setCursor(column, row);
    text.putChar(glyph);
}

void resetColumn(Column& column) {
    column.head = -1;
    column.length = static_cast<std::uint8_t>(6 + randomBelow(14));
    column.speed = static_cast<std::uint8_t>(1 + randomBelow(4));
    column.phase = static_cast<std::uint8_t>(randomBelow(column.speed));
    column.pause = static_cast<std::uint8_t>(4 + randomBelow(48));
}

void advanceColumn(picottl::Text& text, std::int32_t columnIndex) {
    Column& column = gColumns[static_cast<std::size_t>(columnIndex)];

    if (++column.phase < column.speed) {
        return;
    }
    column.phase = 0;

    if (column.pause != 0) {
        --column.pause;
        return;
    }

    const std::int32_t previousHead = column.head;
    ++column.head;

    if (previousHead >= 0 && previousHead < kRows) {
        drawCell(text, columnIndex, previousHead,
                 gCells[static_cast<std::size_t>(columnIndex)]
                       [static_cast<std::size_t>(previousHead)],
                 kNormal);
    }

    if (column.head >= 0 && column.head < kRows) {
        const auto glyph = randomGlyph();
        gCells[static_cast<std::size_t>(columnIndex)]
              [static_cast<std::size_t>(column.head)] = glyph;
        drawCell(text, columnIndex, column.head, glyph, kBright);
    }

    const std::int32_t tail = column.head - column.length;
    if (tail >= 0 && tail < kRows) {
        drawCell(text, columnIndex, tail, ' ', kBlack);
    }

    if (column.head > 1 && column.head < kRows && randomBelow(4) == 0) {
        const std::int32_t visibleTail = column.head - column.length + 1;
        const std::int32_t firstRow = visibleTail > 0 ? visibleTail : 0;
        const std::int32_t bodyRows = column.head - firstRow;
        if (bodyRows > 0) {
            const std::int32_t row = firstRow +
                static_cast<std::int32_t>(randomBelow(
                    static_cast<std::uint32_t>(bodyRows)));
            const auto glyph = randomGlyph();
            gCells[static_cast<std::size_t>(columnIndex)]
                  [static_cast<std::size_t>(row)] = glyph;
            drawCell(text, columnIndex, row, glyph, kNormal);
        }
    }

    if (tail >= kRows) {
        resetColumn(column);
    }
}

} // namespace

int main() {
    static_assert(picottl::modeWidth(kMode) / 9 == kColumns);
    static_assert(picottl::modeHeight(kMode) / 14 == kRows);

    set_sys_clock_khz(130'000, true);

    picottl::PackedFramebuffer2 framebuffer(
        gFramebufferStorage, sizeof gFramebufferStorage,
        picottl::modeWidth(kMode), picottl::modeHeight(kMode));

    picottl::rp2350::MdaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.pixelPins[0] = 19; // VIDEO
    config.pixelPins[1] = 20; // INTENSITY
    config.framebuffer = &framebuffer;

    picottl::rp2350::MdaDisplayDevice device(config);
    picottl::Display display(device);
    picottl::Text text(display.graphics(), picottl::fonts::kIbmMda9x14);
    text.clear();

    for (Column& column : gColumns) {
        resetColumn(column);
        column.pause = static_cast<std::uint8_t>(randomBelow(32));
    }

    const bool ok = display.begin(kMode);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    while (true) {
        if (ok) {
            for (std::int32_t column = 0; column < kColumns; ++column) {
                advanceColumn(text, column);
            }
        }
        sleep_ms(ok ? kFrameDelayMs : 100);
    }
}