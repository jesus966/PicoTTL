// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#define PioScanlineEngine HighGpioScanlineEngine
#include "picottl/rp2350/PioScanlineEngine.hpp"
#undef PioScanlineEngine
#include "support/pico_sdk/RecordingPicoSdk.hpp"

namespace {
using picottl::rp2350::HighGpioScanlineEngine;
namespace sdk = picottl_test::sdk;
constexpr picottl::VideoTiming timing{10'000'000, 8, 4, 4, 4, 4, 1, 1, 1,
    picottl::SyncPolarity::ActiveLow, picottl::SyncPolarity::ActiveHigh};
struct Fixture {
    std::uint32_t table[64]{};
    alignas(4) std::uint8_t pixels[32]{};
    HighGpioScanlineEngine::Config config(unsigned sync, unsigned video, unsigned pio = 0) {
        HighGpioScanlineEngine::Config c;
        c.hsyncPin = static_cast<std::uint8_t>(sync);
        c.videoPin = static_cast<std::uint8_t>(video);
        c.pioInstance = static_cast<std::uint8_t>(pio);
        c.timingBuffer = table; c.timingBufferWords = 64;
        return c;
    }
};
void noClaimsOrPins() {
    CHECK_EQ(sdk::recording.gpioConfigurations, 0u);
    CHECK_EQ(sdk::recording.pinDirections, 0u);
    CHECK_EQ(sdk::recording.pinWrites, 0u);
    CHECK_EQ(sdk::recording.padConfigurations, 0u);
    for (bool claimed : sdk::recording.dmaClaimed) CHECK(!claimed);
    for (const auto& pio : sdk::recording.smClaimed) for (bool claimed : pio) CHECK(!claimed);
    for (const auto& pio : sdk::recording.programs) for (bool loaded : pio) CHECK(!loaded);
}
}

PICOTTL_TEST(engineBRespectsAbsolutePinsAndSharedWindow) {
    struct Case { unsigned base, sync, video, depth; bool valid; };
    const Case cases[] = {
        {0, 2, 16, 4, true}, {0, 2, 31, 1, true},
        {0, 2, 32, 1, false}, {0, 2, 34, 4, false},
        {16, 34, 40, 4, true}, {16, 44, 47, 1, true},
        {16, 45, 16, 1, true}, {16, 34, 42, 8, false},
        {16, 34, 42, 4, true}, {16, 34, 48, 1, false},
        {16, 2, 34, 4, false}, {16, 34, 15, 1, false},
        {0, 20, 31, 2, false}, {16, 20, 31, 2, true},
        {0, 30, 20, 1, false}, {16, 30, 20, 1, true},
        {16, 31, 40, 4, true}, {16, 32, 40, 4, true},
        {16, 46, 20, 1, false}, {16, 47, 20, 1, false}
    };
    for (unsigned pio = 0; pio < 3; ++pio) for (const auto& c : cases) {
        sdk::reset(); Fixture f; sdk::recording.gpioBase[pio] = c.base;
        {
            HighGpioScanlineEngine engine(f.config(c.sync, c.video, pio));
            CHECK(engine.setPixelSource(f.pixels, 4u * c.depth, static_cast<std::uint8_t>(c.depth)));
            CHECK_EQ(engine.configure(timing), c.valid);
            if (c.valid) {
                CHECK_EQ(sdk::recording.outRelative[pio][0], c.sync - c.base);
                CHECK_EQ(sdk::recording.outRelative[pio][1], c.video - c.base);
                CHECK_EQ(sdk::recording.inRelative[pio][1], c.sync + 2 - c.base);
                for (unsigned pin = 0; pin < 48; ++pin) {
                    const bool used = (pin >= c.sync && pin < c.sync + 3) ||
                        (pin >= c.video && pin < c.video + c.depth);
                    CHECK_EQ(sdk::recording.gpioFunction[pin], used ? pio + 1 : 0);
                }
                CHECK(engine.start()); engine.stop(); CHECK(engine.start()); engine.stop();
                CHECK_EQ(sdk::recording.abortWhileEnabled, 0u);
            } else {
                CHECK(!engine.start()); CHECK(!engine.isRunning()); noClaimsOrPins();
            }
            CHECK_EQ(sdk::recording.invalidPinCalls, 0u);
            CHECK_EQ(sdk::recording.gpioBase[pio], c.base);
        }
        for (bool claimed : sdk::recording.dmaClaimed) CHECK(!claimed);
        for (bool claimed : sdk::recording.smClaimed[pio]) CHECK(!claimed);
        for (bool loaded : sdk::recording.programs[pio]) CHECK(!loaded);
    }
}

PICOTTL_TEST(engineBHighIdleLevelsAndDirectionsAreNotTruncated) {
    sdk::reset(); Fixture f; sdk::recording.gpioBase[0] = 16;
    HighGpioScanlineEngine engine(f.config(34, 40));
    CHECK(engine.setPixelSource(f.pixels, 16, 4)); CHECK(engine.configure(timing));
    CHECK_EQ(sdk::recording.outputMask[0], 0x0f1c0000u);
    CHECK_EQ(sdk::recording.outputValues[0], 0x00040000u);
    CHECK_EQ(sdk::recording.directionMask[0], 0x0f1c0000u);
    CHECK(engine.start()); engine.stop(); CHECK(engine.start()); engine.stop();
    CHECK_EQ(sdk::recording.outputMask[0], 0x0f1c0000u);
    CHECK_EQ(sdk::recording.outputValues[0], 0x00040000u);
    CHECK_EQ((dma_hw->ch[0].ctrl_trig & DMA_CH0_CTRL_TRIG_EN_BITS), 0u);
    CHECK_EQ(sdk::recording.invalidPinCalls, 0u);
}

PICOTTL_TEST(engineBSignalOnlyCanReachGpio47) {
    for (unsigned sync : {30u, 31u, 32u, 34u, 46u, 47u}) {
        sdk::reset(); Fixture f; sdk::recording.gpioBase[0] = 16;
        HighGpioScanlineEngine engine(f.config(sync, 0));
        CHECK_EQ(engine.configure(timing), sync != 47);
        if (sync == 47) { CHECK(!engine.start()); noClaimsOrPins(); }
        else { CHECK(engine.start()); engine.stop(); }
        if (sync == 46) {
            CHECK_EQ(sdk::recording.outputMask[0], 0xc0000000u);
            CHECK_EQ(sdk::recording.outputValues[0], 0x40000000u);
        }
        CHECK_EQ(sdk::recording.invalidPinCalls, 0u);
    }
}

PICOTTL_TEST(engineBRejectedWindowCanRetryWithoutChangingForeignResources) {
    sdk::reset(); Fixture f; sdk::recording.gpioBase[0] = 16;
    sdk::recording.smClaimed[0][3] = true;
    sdk::recording.smEnabled[0][3] = true;
    sdk::recording.dmaClaimed[9] = true;
    sdk::recording.programs[0][4] = true;
    {
        // No pixels: sync pins fit. Adding a framebuffer would require GPIO 2.
        HighGpioScanlineEngine engine(f.config(34, 2));
        CHECK(engine.configure(timing)); CHECK(engine.start()); engine.stop();
        const auto gpioCalls = sdk::recording.gpioConfigurations;
        CHECK(engine.setPixelSource(f.pixels, 4));
        for (unsigned repeat = 0; repeat < 3; ++repeat) {
            CHECK(!engine.configure(timing)); CHECK(!engine.start());
            CHECK_EQ(sdk::recording.gpioConfigurations, gpioCalls);
            CHECK_EQ(sdk::recording.gpioBase[0], 16u);
            CHECK(sdk::recording.smEnabled[0][3]);
            CHECK(sdk::recording.dmaClaimed[9]);
            CHECK(sdk::recording.programs[0][4]);
        }
        CHECK(engine.setPixelSource(nullptr, 0));
        CHECK(engine.configure(timing)); CHECK(engine.start()); engine.stop();
    }
    CHECK(sdk::recording.smClaimed[0][3]); CHECK(sdk::recording.smEnabled[0][3]);
    CHECK(sdk::recording.dmaClaimed[9]); CHECK(sdk::recording.programs[0][4]);
    CHECK(!sdk::recording.smClaimed[0][0]); CHECK(!sdk::recording.dmaClaimed[0]);
}

PICOTTL_TEST(engineBSuccessiveLowHighLowSessionsKeepApplicationSelectedBase) {
    for (unsigned base : {0u, 16u, 0u}) {
        sdk::reset(); Fixture f; sdk::recording.gpioBase[0] = base;
        {
            HighGpioScanlineEngine engine(f.config(base + 2, base + 8));
            CHECK(engine.setPixelSource(f.pixels, 8, 2));
            CHECK(engine.configure(timing)); CHECK(engine.start()); engine.stop();
            CHECK(engine.configure(timing)); CHECK(engine.start()); engine.stop();
            CHECK_EQ(sdk::recording.gpioBase[0], base);
        }
        CHECK_EQ(sdk::recording.gpioBase[0], base);
        for (bool loaded : sdk::recording.programs[0]) CHECK(!loaded);
    }
}
