// PicoTTL - host tests: Terminal and HostDisplayDevice
//
// Validates the terminal state machine on the host backend: cell-buffer
// truth, non-destructive overlays, timestamp-pure update(), repaint()
// reconstruction and auto-scroll attribute preservation - the automated
// counterpart of hardware Example 23, plus the host device contract.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "framework/test.hpp"
#include "picottl/Display.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/PackedFramebuffer.hpp"
#include "picottl/Terminal.hpp"
#include "picottl/Text.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"
#include "picottl/host/HostDisplayDevice.hpp"

using picottl::CellAttributes;
using picottl::Color;
using picottl::CursorStyle;

namespace {

constexpr std::uint16_t kW = 720;
constexpr std::uint16_t kH = 350;

struct Fixture {
    std::uint8_t storage[picottl::PackedFramebuffer2::bytesFor(kW, kH)] = {};
    picottl::PackedFramebuffer2 fb{storage, sizeof storage, kW, kH};
    picottl::Graphics gfx{&fb};
    picottl::Text text{gfx, picottl::fonts::kIbmMda9x14};
    picottl::TerminalCell cells[picottl::Terminal::cellsFor(80, 25)];
    picottl::Terminal terminal{text, cells,
                               sizeof cells / sizeof cells[0]};
};

} // namespace

PICOTTL_TEST(hostDeviceHonorsTheBackendContract) {
    std::uint8_t storage[picottl::MonochromeFramebuffer::bytesFor(kW, kH)] = {};
    picottl::MonochromeFramebuffer fb(storage, sizeof storage, kW, kH);
    picottl::host::HostDisplayDevice::Config config;
    config.framebuffer = &fb;
    picottl::host::HostDisplayDevice device(config);
    picottl::Display display(device);

    CHECK(display.supports(picottl::VideoMode::IbmMda));
    CHECK(display.supportsPixelFormat(picottl::PixelFormat::Packed6));
    CHECK(display.begin(picottl::VideoMode::IbmMda));
    CHECK(display.isActive());
    CHECK_EQ(display.width(), 720);
    CHECK_EQ(display.height(), 350);
    CHECK(display.begin(picottl::VideoMode::IbmMda)); // same mode: true
    CHECK(display.surface() == &fb);
    display.end();
    CHECK(!display.isActive());

    // Wrong framebuffer geometry must fail explicitly.
    std::uint8_t tiny[picottl::MonochromeFramebuffer::bytesFor(8, 8)] = {};
    picottl::MonochromeFramebuffer tinyFb(tiny, sizeof tiny, 8, 8);
    picottl::host::HostDisplayDevice::Config badConfig;
    badConfig.framebuffer = &tinyFb;
    picottl::host::HostDisplayDevice badDevice(badConfig);
    CHECK(!badDevice.begin(picottl::VideoMode::IbmMda));
}

PICOTTL_TEST(hostDeviceSwitchesModesThroughThePublicApi) {
    static std::uint8_t ibmStorage[picottl::MonochromeFramebuffer::bytesFor(
        picottl::modeWidth(picottl::VideoMode::IbmMda),
        picottl::modeHeight(picottl::VideoMode::IbmMda))] = {};
    picottl::MonochromeFramebuffer ibmFb(
        ibmStorage, sizeof ibmStorage,
        picottl::modeWidth(picottl::VideoMode::IbmMda),
        picottl::modeHeight(picottl::VideoMode::IbmMda));
    static std::uint8_t overStorage[picottl::MonochromeFramebuffer::bytesFor(
        picottl::modeWidth(picottl::VideoMode::MdaOverscan),
        picottl::modeHeight(picottl::VideoMode::MdaOverscan))] = {};
    picottl::MonochromeFramebuffer overFb(
        overStorage, sizeof overStorage,
        picottl::modeWidth(picottl::VideoMode::MdaOverscan),
        picottl::modeHeight(picottl::VideoMode::MdaOverscan));

    picottl::host::HostDisplayDevice::Config config;
    config.framebuffer = &ibmFb;
    picottl::host::HostDisplayDevice device(config);
    picottl::Display display(device);

    CHECK(display.supports(picottl::VideoMode::MdaOverscan));
    CHECK(display.begin(picottl::VideoMode::IbmMda));
    CHECK(!display.begin(picottl::VideoMode::MdaOverscan)); // active: end() first
    CHECK(!display.setFramebuffer(&overFb));                // active: rejected

    display.end();
    CHECK(display.setFramebuffer(&overFb)); // mode and framebuffer are
    CHECK(display.begin(picottl::VideoMode::MdaOverscan)); // separate steps
    CHECK_EQ(display.width(),
             picottl::modeWidth(picottl::VideoMode::MdaOverscan));
    // Graphics is rebound by begin() and derives geometry live.
    CHECK_EQ(display.graphics().width(),
             picottl::modeWidth(picottl::VideoMode::MdaOverscan));
    CHECK_EQ(display.graphics().height(),
             picottl::modeHeight(picottl::VideoMode::MdaOverscan));
}

PICOTTL_TEST(terminalDerivesGeometryAndRejectsSmallBuffers) {
    Fixture f;
    CHECK(f.terminal.isValid());
    CHECK_EQ(f.terminal.columns(), 80);
    CHECK_EQ(f.terminal.rows(), 25);

    picottl::TerminalCell tooFew[10];
    picottl::Terminal inert(f.text, tooFew, 10);
    CHECK(!inert.isValid());
    inert.print("must not crash");
}

PICOTTL_TEST(cellBufferMirrorsWrites) {
    Fixture f;
    f.terminal.setAttributes({Color{3}, Color{0}, true, false});
    f.terminal.setCursor(4, 2);
    f.terminal.print("Hi");
    CHECK_EQ(f.cells[2 * 80 + 4].character, 'H');
    CHECK_EQ(f.cells[2 * 80 + 5].character, 'i');
    CHECK_EQ(f.cells[2 * 80 + 4].attributes.foreground.index, 3);
    CHECK(f.cells[2 * 80 + 4].attributes.underline);
}

PICOTTL_TEST(cursorOverlayIsNonDestructive) {
    Fixture f;
    f.terminal.print("hello world");
    f.terminal.setCursorFollowsOutput(false);
    f.terminal.setCursorPosition(3, 0);

    std::uint8_t before[sizeof f.storage];
    std::memcpy(before, f.storage, sizeof before);

    f.terminal.setCursorStyle(CursorStyle::Block);
    CHECK(std::memcmp(before, f.storage, sizeof before) != 0); // drawn
    f.terminal.setCursorStyle(CursorStyle::Underline);
    f.terminal.setCursorStyle(CursorStyle::None);
    CHECK_EQ(std::memcmp(before, f.storage, sizeof before), 0); // restored
}

PICOTTL_TEST(updateIsAPureFunctionOfTheTimestamp) {
    Fixture a;
    Fixture b;
    for (picottl::Terminal* t : {&a.terminal, &b.terminal}) {
        t->setAttributes({Color{1}, Color{0}, false, true}); // blink
        t->print("BLINK");
        t->setCursorStyle(CursorStyle::Block);
    }
    // Same final timestamp through different call cadences.
    a.terminal.update(0);
    a.terminal.update(400);
    a.terminal.update(600);
    a.terminal.update(2100);
    b.terminal.update(2100);
    CHECK_EQ(std::memcmp(a.storage, b.storage, sizeof a.storage), 0);
}

PICOTTL_TEST(blinkHidesGlyphAndKeepsBackground) {
    Fixture f;
    f.terminal.setAttributes({Color{1}, Color{2}, false, true});
    f.terminal.print("X");
    f.terminal.update(0); // phase on (0 % 1000 < 500)

    std::uint8_t visible[sizeof f.storage];
    std::memcpy(visible, f.storage, sizeof visible);

    f.terminal.update(600); // phase off
    CHECK(std::memcmp(visible, f.storage, sizeof visible) != 0);
    CHECK_EQ(f.fb.pixel(1, 1).index, 2); // background remains everywhere

    f.terminal.update(1100); // phase on again: pixel-identical
    CHECK_EQ(std::memcmp(visible, f.storage, sizeof visible), 0);
}

PICOTTL_TEST(repaintReconstructsFromCellBuffer) {
    Fixture f;
    f.terminal.setAttributes({Color{3}, Color{0}, true, false});
    f.terminal.print("precious content");
    std::uint8_t reference[sizeof f.storage];
    std::memcpy(reference, f.storage, sizeof reference);

    f.gfx.clear(Color{2}); // wholesale pixel corruption
    CHECK(std::memcmp(reference, f.storage, sizeof reference) != 0);

    f.terminal.repaint(); // the framebuffer is only a cache
    CHECK_EQ(std::memcmp(reference, f.storage, sizeof reference), 0);
}

PICOTTL_TEST(autoScrollPreservesCellsAndAttributes) {
    Fixture f;
    f.terminal.setAttributes({Color{3}, Color{0}, false, false});
    f.terminal.setCursor(0, 24);
    f.terminal.print("bottom");
    f.terminal.setAttributes({Color{1}, Color{0}, false, false});
    f.terminal.print("\nnext"); // forces one scroll
    CHECK_EQ(f.terminal.cursorY(), 24);
    CHECK_EQ(f.cells[23 * 80 + 0].character, 'b'); // moved up one row
    CHECK_EQ(f.cells[23 * 80 + 0].attributes.foreground.index, 3);
    CHECK_EQ(f.cells[24 * 80 + 0].character, 'n');
}

PICOTTL_TEST(clearOperationsAffectExactRanges) {
    Fixture f;
    f.terminal.setCursor(0, 0);
    f.terminal.print("0123456789");
    f.terminal.setCursor(4, 0);
    f.terminal.clearToEndOfLine();
    CHECK_EQ(f.cells[3].character, '3');
    CHECK_EQ(f.cells[4].character, ' ');
    CHECK_EQ(f.cells[79].character, ' ');
    f.terminal.setCursor(0, 0);
    f.terminal.print("ab");
    f.terminal.clearLine();
    CHECK_EQ(f.cells[0].character, ' ');
    CHECK_EQ(f.cells[1].character, ' ');
}
