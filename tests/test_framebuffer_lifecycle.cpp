// PicoTTL - regression tests for framebuffer ownership at the engine boundary.
// Real MDA/CGA/EGA backends run against a recording engine, not real DMA/PIO.
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <initializer_list>

#include "framework/test.hpp"
#include "support/RecordingScanlineEngine.hpp"

#include "picottl/Display.hpp"
#include "picottl/PackedFramebuffer.hpp"
#include "picottl/rp2350/CgaDisplayDevice.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

namespace {

using namespace picottl;
using picottl_test::engineRecording;

// A single reusable pair of aligned host buffers covers every tested mode.
alignas(std::uint32_t) std::uint8_t storageA[224'000];
alignas(std::uint32_t) std::uint8_t storageB[224'000];

rp2350::MdaDisplayDevice::Config configFor(rp2350::MdaDisplayDevice*) {
    rp2350::MdaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.videoPin = 16;
    return config;
}

rp2350::CgaDisplayDevice::Config configFor(rp2350::CgaDisplayDevice*) {
    rp2350::CgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3;
    config.bluePin = 16;
    config.greenPin = 17;
    config.redPin = 18;
    config.intensityPin = 19;
    return config;
}

rp2350::EgaDisplayDevice::Config configFor(rp2350::EgaDisplayDevice*) {
    rp2350::EgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3;
    config.primaryBluePin = 16;
    config.primaryGreenPin = 17;
    config.primaryRedPin = 18;
    config.secondaryBluePin = 19;
    config.secondaryGreenPin = 20;
    config.secondaryRedPin = 21;
    return config;
}

template <typename Device, unsigned Bpp>
void detachAndRestart(VideoMode mode, VideoMode otherMode) {
    auto config = configFor(static_cast<Device*>(nullptr));
    Device device(config);
    Display display(device);
    {
        PackedFramebuffer<Bpp> framebuffer(storageA, sizeof storageA,
                                            modeWidth(mode), modeHeight(mode));
        CHECK(display.setFramebuffer(&framebuffer));
        CHECK(display.begin(mode));
        CHECK(engineRecording().source == storageA);
        CHECK(engineRecording().startedSource == storageA);
        CHECK_EQ(engineRecording().bitsPerPixel, Bpp);
        CHECK_EQ(engineRecording().pinCount, Bpp == 8 ? 6u : Bpp);
        CHECK(!display.setFramebuffer(nullptr)); // active: leave source intact
        CHECK(engineRecording().source == storageA);
        display.end();
        CHECK(!engineRecording().running);
        CHECK(display.setFramebuffer(nullptr));
        CHECK(display.surface() == nullptr);
        CHECK(engineRecording().source == nullptr); // invalidated immediately
        CHECK_EQ(engineRecording().sourceBytes, 0u);
    } // The old framebuffer object is no longer alive.

    CHECK(display.setFramebuffer(nullptr)); // idempotent while inactive
    // A rejected mode must not restore the old source.
    CHECK(!display.begin(static_cast<VideoMode>(255)));
    CHECK(engineRecording().source == nullptr);
    for (VideoMode next : {mode, otherMode, mode}) {
        CHECK(display.begin(next));
        CHECK(engineRecording().configuredSource == nullptr);
        CHECK(engineRecording().startedSource == nullptr);
        CHECK(display.surface() == nullptr);
        CHECK(!display.graphics().isBound());
        storageA[0] = 0x5A;
        display.graphics().clear(); // no framebuffer: safe no-op after begin
        CHECK_EQ(storageA[0], 0x5A);
        CHECK(!display.setFramebuffer(nullptr)); // still rejected while active
        display.end();
    }
}

template <typename Device, unsigned Bpp>
void replaceAndChangeGeometry(VideoMode mode, VideoMode otherMode) {
    PackedFramebuffer<Bpp> first(storageA, sizeof storageA,
                                  modeWidth(mode), modeHeight(mode));
    PackedFramebuffer<Bpp> replacement(storageB, sizeof storageB,
                                        modeWidth(mode), modeHeight(mode));
    PackedFramebuffer<Bpp> resized(storageB, sizeof storageB,
                                    modeWidth(otherMode), modeHeight(otherMode));
    auto config = configFor(static_cast<Device*>(nullptr));
    config.framebuffer = &first; // constructor-provided binding also works
    Device device(config);
    CHECK(device.begin(mode));
    CHECK(engineRecording().startedSource == storageA);
    CHECK(!device.setFramebuffer(&replacement));
    CHECK(device.surface() == &first);
    CHECK(engineRecording().source == storageA);
    device.end();

    CHECK(device.setFramebuffer(&replacement));
    CHECK(engineRecording().source == nullptr); // old storage can be retired
    CHECK(device.begin(mode));
    CHECK(device.surface() == &replacement);
    CHECK(engineRecording().configuredSource == storageB);
    CHECK(engineRecording().startedSource == storageB);
    CHECK_EQ(engineRecording().sourceBytes, replacement.sizeBytes());
    device.end();

    CHECK(device.setFramebuffer(&resized));
    const unsigned starts = engineRecording().starts;
    CHECK(!device.begin(mode)); // mismatched geometry cannot start old source
    CHECK_EQ(engineRecording().starts, starts);
    CHECK(engineRecording().source == nullptr);
    CHECK(device.begin(otherMode));
    CHECK(engineRecording().startedSource == storageB);
    CHECK_EQ(engineRecording().sourceBytes, resized.sizeBytes());
    CHECK_EQ(engineRecording().bitsPerPixel, Bpp);
    CHECK_EQ(engineRecording().pinCount, Bpp == 8 ? 6u : Bpp);
    device.end();
    CHECK(device.setFramebuffer(nullptr));
    CHECK(device.begin(mode)); // old resized source must not constrain geometry
    CHECK(engineRecording().startedSource == nullptr);
    device.end();
}

template <typename Device>
void initiallyWithoutFramebuffer(VideoMode mode, VideoMode otherMode) {
    Device device(configFor(static_cast<Device*>(nullptr)));
    for (VideoMode next : {mode, otherMode, mode}) {
        CHECK(device.begin(next));
        CHECK(device.surface() == nullptr);
        CHECK(engineRecording().configuredSource == nullptr);
        CHECK(engineRecording().startedSource == nullptr);
        device.end();
    }
}

template <typename Device, unsigned HighBpp, unsigned LowBpp>
void changeDepthOnSameDevice(VideoMode mode, VideoMode otherMode) {
    PackedFramebuffer<HighBpp> high(storageA, sizeof storageA,
                                     modeWidth(mode), modeHeight(mode));
    PackedFramebuffer<LowBpp> low(storageB, sizeof storageB,
                                   modeWidth(otherMode), modeHeight(otherMode));
    Device device(configFor(static_cast<Device*>(nullptr)));
    Framebuffer* const frames[] = {&high, &low, &high};
    const VideoMode modes[] = {mode, otherMode, mode};
    const unsigned depths[] = {HighBpp, LowBpp, HighBpp};

    for (unsigned step = 0; step < 3; ++step) {
        CHECK(!device.isActive());
        CHECK(!engineRecording().running);
        const unsigned starts = engineRecording().starts;
        CHECK(device.setFramebuffer(frames[step]));
        CHECK(!device.isActive());
        CHECK(!engineRecording().running);
        CHECK(engineRecording().source == nullptr);
        CHECK_EQ(engineRecording().sourceBytes, 0u);
        CHECK_EQ(engineRecording().bitsPerPixel, 1u);
        CHECK_EQ(engineRecording().pinCount, 1u);
        CHECK_EQ(engineRecording().starts, starts);

        CHECK(device.begin(modes[step]));
        CHECK(device.isActive());
        CHECK(device.surface() == frames[step]);
        CHECK_EQ(device.width(), modeWidth(modes[step]));
        CHECK_EQ(device.height(), modeHeight(modes[step]));
        CHECK(engineRecording().running);
        CHECK_EQ(engineRecording().starts, starts + 1u);
        CHECK(engineRecording().source == frames[step]->data());
        CHECK(engineRecording().configuredSource == frames[step]->data());
        CHECK(engineRecording().startedSource == frames[step]->data());
        CHECK_EQ(engineRecording().sourceBytes, frames[step]->sizeBytes());
        CHECK_EQ(engineRecording().sourceBytes,
                 (static_cast<std::size_t>(modeWidth(modes[step])) *
                      depths[step] + 7u) / 8u * modeHeight(modes[step]));
        CHECK_EQ(engineRecording().bitsPerPixel, depths[step]);
        CHECK_EQ(engineRecording().pinCount,
                 depths[step] == 8 ? 6u : depths[step]);

        // A depth change while active must preserve the current binding.
        CHECK(!device.setFramebuffer(step == 1 ? frames[0] : frames[1]));
        CHECK(device.surface() == frames[step]);
        CHECK(engineRecording().source == frames[step]->data());
        CHECK_EQ(engineRecording().bitsPerPixel, depths[step]);
        device.end();
        CHECK(!device.isActive());
        CHECK(!engineRecording().running);
    }
}

} // namespace

PICOTTL_TEST(mdaFramebufferDetachAndRestart) {
    detachAndRestart<rp2350::MdaDisplayDevice, 1>(VideoMode::IbmMda,
                                                VideoMode::MdaOverscan);
    detachAndRestart<rp2350::MdaDisplayDevice, 2>(VideoMode::MdaOverscan,
                                                VideoMode::IbmMda);
}

PICOTTL_TEST(cgaFramebufferDetachAndRestart) {
    detachAndRestart<rp2350::CgaDisplayDevice, 1>(VideoMode::IbmCga640,
                                                VideoMode::IbmCga320);
    detachAndRestart<rp2350::CgaDisplayDevice, 2>(VideoMode::IbmCga320,
                                                VideoMode::IbmCga640);
    detachAndRestart<rp2350::CgaDisplayDevice, 4>(VideoMode::IbmCga640,
                                                VideoMode::IbmCga320);
}

PICOTTL_TEST(egaFramebufferDetachAndRestart) {
    detachAndRestart<rp2350::EgaDisplayDevice, 1>(VideoMode::IbmEga350,
                                                VideoMode::IbmCga640);
    detachAndRestart<rp2350::EgaDisplayDevice, 2>(VideoMode::IbmCga640,
                                                VideoMode::IbmCga320);
    detachAndRestart<rp2350::EgaDisplayDevice, 4>(VideoMode::IbmCga320,
                                                VideoMode::IbmEga350);
    detachAndRestart<rp2350::EgaDisplayDevice, 8>(VideoMode::IbmEga350,
                                                VideoMode::IbmCga320);
}

PICOTTL_TEST(mdaFramebufferReplacementAndGeometry) {
    replaceAndChangeGeometry<rp2350::MdaDisplayDevice, 1>(
        VideoMode::IbmMda, VideoMode::MdaOverscan);
    replaceAndChangeGeometry<rp2350::MdaDisplayDevice, 2>(
        VideoMode::MdaOverscan, VideoMode::IbmMda);
}

PICOTTL_TEST(cgaFramebufferReplacementAndGeometry) {
    replaceAndChangeGeometry<rp2350::CgaDisplayDevice, 1>(
        VideoMode::IbmCga640, VideoMode::IbmCga320);
    replaceAndChangeGeometry<rp2350::CgaDisplayDevice, 2>(
        VideoMode::IbmCga320, VideoMode::IbmCga640);
    replaceAndChangeGeometry<rp2350::CgaDisplayDevice, 4>(
        VideoMode::IbmCga640, VideoMode::IbmCga320);
}

PICOTTL_TEST(egaFramebufferReplacementAndGeometry) {
    replaceAndChangeGeometry<rp2350::EgaDisplayDevice, 1>(
        VideoMode::IbmEga350, VideoMode::IbmCga640);
    replaceAndChangeGeometry<rp2350::EgaDisplayDevice, 2>(
        VideoMode::IbmCga640, VideoMode::IbmCga320);
    replaceAndChangeGeometry<rp2350::EgaDisplayDevice, 4>(
        VideoMode::IbmCga320, VideoMode::IbmEga350);
    replaceAndChangeGeometry<rp2350::EgaDisplayDevice, 8>(
        VideoMode::IbmEga350, VideoMode::IbmCga320);
}

PICOTTL_TEST(mdaStartsAndRestartsWithoutFramebuffer) {
    initiallyWithoutFramebuffer<rp2350::MdaDisplayDevice>(
        VideoMode::IbmMda, VideoMode::MdaOverscan);
}

PICOTTL_TEST(cgaStartsAndRestartsWithoutFramebuffer) {
    initiallyWithoutFramebuffer<rp2350::CgaDisplayDevice>(
        VideoMode::IbmCga640, VideoMode::IbmCga320);
}

PICOTTL_TEST(egaStartsAndRestartsWithoutFramebuffer) {
    initiallyWithoutFramebuffer<rp2350::EgaDisplayDevice>(
        VideoMode::IbmEga350, VideoMode::IbmCga640);
}

PICOTTL_TEST(mdaChangesDepthOnSameDevice) {
    changeDepthOnSameDevice<rp2350::MdaDisplayDevice, 2, 1>(
        VideoMode::IbmMda, VideoMode::MdaOverscan);
    changeDepthOnSameDevice<rp2350::MdaDisplayDevice, 2, 1>(
        VideoMode::MdaOverscan, VideoMode::IbmMda);
}

PICOTTL_TEST(cgaChangesDepthOnSameDevice) {
    changeDepthOnSameDevice<rp2350::CgaDisplayDevice, 4, 1>(
        VideoMode::IbmCga640, VideoMode::IbmCga320);
    changeDepthOnSameDevice<rp2350::CgaDisplayDevice, 4, 1>(
        VideoMode::IbmCga320, VideoMode::IbmCga640);
}

PICOTTL_TEST(egaChangesDepthOnSameDevice) {
    changeDepthOnSameDevice<rp2350::EgaDisplayDevice, 8, 4>(
        VideoMode::IbmEga350, VideoMode::IbmCga640);
    changeDepthOnSameDevice<rp2350::EgaDisplayDevice, 8, 4>(
        VideoMode::IbmCga640, VideoMode::IbmCga320);
    changeDepthOnSameDevice<rp2350::EgaDisplayDevice, 8, 4>(
        VideoMode::IbmCga320, VideoMode::IbmEga350);
}
