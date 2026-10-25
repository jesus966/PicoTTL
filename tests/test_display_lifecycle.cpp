// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#include "picottl/Display.hpp"
#include "picottl/PackedFramebuffer.hpp"
#include "picottl/Text.hpp"
#include "picottl/fonts/IbmCga8x8.hpp"
#include "picottl/host/HostDisplayDevice.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

#include <algorithm>
#include <array>
#include <memory>

namespace {
using picottl::Color;
using picottl::VideoMode;
using Frame = picottl::PackedFramebuffer2;
using Device = picottl::host::HostDisplayDevice;
constexpr auto mode = VideoMode::IbmMda;
using Storage = std::array<std::uint8_t, Frame::bytesFor(720, 350)>;

struct Fixture {
    Storage a{}, b{};
    Frame first{a.data(), a.size(), 720, 350};
    Frame second{b.data(), b.size(), 720, 350};
    Device device{Device::Config{&first}};
    picottl::Display display{device};
};
} // namespace

PICOTTL_TEST(displayDetachImmediatelyMakesDrawingInert) {
    Fixture f;
    auto& graphics = f.display.graphics();
    f.first.fill(Color{1});
    CHECK(f.display.begin(mode));
    f.display.end();
    CHECK(f.display.setFramebuffer(nullptr));
    CHECK(f.display.surface() == nullptr);
    CHECK(!graphics.isBound());
    graphics.drawPixel(0, 0, Color{3});
    graphics.clear();
    CHECK(std::all_of(f.a.begin(), f.a.end(), [](std::uint8_t b) { return b == 0x55; }));
    CHECK_EQ(graphics.width(), 0);
    CHECK_EQ(graphics.height(), 0);
}

PICOTTL_TEST(displayReplacementUpdatesRetainedGraphicsAndTextBeforeBegin) {
    Fixture f;
    auto& graphics = f.display.graphics();
    picottl::Text text(graphics, picottl::fonts::kIbmCga8x8);
    CHECK(f.display.begin(mode)); f.display.end();
    CHECK(f.display.setFramebuffer(&f.second));
    CHECK(f.display.surface() == &f.second);
    CHECK(&f.display.graphics() == &graphics);
    graphics.drawPixel(0, 0, Color{3});
    CHECK_EQ(f.second.pixel(0, 0).index, 3);
    CHECK_EQ(f.first.pixel(0, 0).index, 0);

    // Compare retained Text output with an independently bound renderer.
    f.second.fill(Color{0});
    Storage expected{};
    Frame reference(expected.data(), expected.size(), 720, 350);
    picottl::Graphics referenceGraphics(&reference);
    picottl::Text referenceText(referenceGraphics, picottl::fonts::kIbmCga8x8);
    text.setColors(Color{3}, Color{1});
    referenceText.setColors(Color{3}, Color{1});
    text.putChar('A'); referenceText.putChar('A');
    CHECK(f.b == expected);
    CHECK(std::all_of(f.a.begin(), f.a.end(), [](std::uint8_t b) { return b == 0; }));
    CHECK(f.display.begin(mode));
    CHECK(&f.display.graphics() == &graphics);
    f.display.end();
}

PICOTTL_TEST(displayRejectedChangesAndEndPreserveDrawingBinding) {
    Fixture f;
    auto& graphics = f.display.graphics();
    CHECK(f.display.begin(mode));
    CHECK(!f.display.setFramebuffer(&f.second));
    CHECK(!f.display.setFramebuffer(nullptr));
    CHECK(f.display.surface() == &f.first);
    CHECK(!f.display.begin(VideoMode::IbmCga640));
    graphics.drawPixel(0, 0, Color{3});
    CHECK_EQ(f.first.pixel(0, 0).index, 3);
    CHECK_EQ(f.second.pixel(0, 0).index, 0);
    f.display.end();
    graphics.drawPixel(1, 0, Color{2});
    CHECK_EQ(f.first.pixel(1, 0).index, 2);
    CHECK(&f.display.graphics() == &graphics);
}

PICOTTL_TEST(displayFailedBeginDoesNotRestoreRetiredSurface) {
    Fixture f;
    picottl::rp2350::MdaDisplayDevice::Config config;
    config.hsyncPin = 2; config.videoPin = 16; config.framebuffer = &f.first;
    // Real backend validation with the existing recording scanline engine.
    picottl::rp2350::MdaDisplayDevice device(config);
    picottl::Display display(device);
    CHECK(display.begin(mode)); display.end();
    CHECK(display.setFramebuffer(nullptr));
    CHECK(!display.begin(VideoMode::IbmCga640)); // valid mode unsupported by MDA
    CHECK(!display.graphics().isBound());
    display.graphics().drawPixel(0, 0, Color{3});
    CHECK_EQ(f.first.pixel(0, 0).index, 0);

    // Mode validation is deferred to begin(), not CPU drawing attachment.
    std::uint8_t pixels[Frame::bytesFor(8, 8)]{};
    Frame small(pixels, sizeof pixels, 8, 8);
    CHECK(display.setFramebuffer(&small));
    CHECK(!display.begin(mode));
    CHECK(display.surface() == &small);
    CHECK_EQ(display.graphics().width(), 8);
    display.graphics().drawPixel(0, 0, Color{2});
    CHECK_EQ(small.pixel(0, 0).index, 2);
    CHECK_EQ(f.first.pixel(0, 0).index, 0);
    CHECK(display.setFramebuffer(nullptr));
}

PICOTTL_TEST(displayDetachedFramebufferCanBeDestroyedBeforeRetainedDrawing) {
    Storage storage{};
    auto framebuffer = std::make_unique<Frame>(storage.data(), storage.size(), 720, 350);
    Device device(Device::Config{framebuffer.get()});
    picottl::Display display(device);
    auto& graphics = display.graphics();
    picottl::Text text(graphics, picottl::fonts::kIbmCga8x8);
    CHECK(display.begin(mode)); display.end();
    CHECK(display.setFramebuffer(nullptr));
    framebuffer.reset();
    CHECK(&display.graphics() == &graphics);
    graphics.drawPixel(0, 0, Color{3});
    graphics.clear();
    text.putChar('A');
    CHECK_EQ(text.columns(), 0);
    CHECK_EQ(text.rows(), 0);
    CHECK(std::all_of(storage.begin(), storage.end(), [](std::uint8_t b) { return b == 0; }));
}

PICOTTL_TEST(displayRepeatedLifecycleKeepsDrawingBindingConsistent) {
    Fixture f;
    CHECK(f.display.setFramebuffer(nullptr));
    auto& graphics = f.display.graphics();
    for (unsigned cycle = 0; cycle < 4; ++cycle) {
        for (Frame* frame : {&f.first, &f.second}) {
            CHECK(f.display.setFramebuffer(frame));
            CHECK(f.display.surface() == frame);
            graphics.drawPixel(2, 0, Color{3});
            CHECK_EQ(frame->pixel(2, 0).index, 3);
            CHECK(f.display.begin(mode)); CHECK(f.display.begin(mode));
            f.display.end(); f.display.end();
            CHECK(f.display.setFramebuffer(nullptr));
            CHECK(f.display.setFramebuffer(nullptr));
            CHECK(!graphics.isBound());
            CHECK(f.display.begin(mode)); // output without pixel storage
            f.display.end();
            CHECK(&f.display.graphics() == &graphics);
        }
    }
}
