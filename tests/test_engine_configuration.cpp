// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#include "support/HardwareScanlineEngine.hpp"
#include "support/pico_sdk/RecordingPicoSdk.hpp"
#include "picottl/video/ModeTimings.hpp"

namespace {
using picottl::rp2350::HardwareScanlineEngine;
using picottl::VideoTiming;
namespace sdk = picottl_test::sdk;

constexpr VideoTiming timing{10'000'000, 8, 4, 4, 4, 4, 1, 1, 1,
    picottl::SyncPolarity::ActiveHigh, picottl::SyncPolarity::ActiveLow};

struct Fixture {
    std::uint32_t table[64]{};
    alignas(std::uint32_t) std::uint8_t pixels[32]{};
    HardwareScanlineEngine::Config config() {
        HardwareScanlineEngine::Config c;
        c.hsyncPin = 2;
        c.videoPin = 16;
        c.timingBuffer = table;
        c.timingBufferWords = 64;
        return c;
    }
};

void checkRejectedStart(HardwareScanlineEngine& engine) {
    const auto calls = sdk::recording.calls;
    CHECK(!engine.start());
    CHECK(!engine.isRunning());
    CHECK_EQ(sdk::recording.calls, calls); // no DMA rearm or PIO access
    if (engine.isRunning()) engine.stop(); // keep a failing test isolated
}

void checkQuiescent(std::uint32_t mask) {
    for (uint ch = 0; ch < NUM_DMA_CHANNELS; ++ch) {
        if ((mask & (1u << ch)) != 0) {
            CHECK_EQ(dma_hw->ch[ch].ctrl_trig &
                (DMA_CH0_CTRL_TRIG_EN_BITS | DMA_CH0_CTRL_TRIG_BUSY_BITS), 0u);
        }
    }
    CHECK(!sdk::recording.smEnabled[0][0]);
    CHECK(!sdk::recording.smEnabled[0][1]);
    CHECK_EQ(sdk::recording.abortWhileEnabled, 0u);
}

void checkReleased() {
    for (uint ch = 0; ch < 4; ++ch) CHECK(!sdk::recording.dmaClaimed[ch]);
    CHECK(!sdk::recording.smClaimed[0][0]);
    CHECK(!sdk::recording.smClaimed[0][1]);
    for (bool loaded : sdk::recording.programs[0]) CHECK(!loaded);
}
} // namespace

PICOTTL_TEST(engineNewConfigurationAndStart) {
    sdk::reset(); Fixture f;
    {
        HardwareScanlineEngine engine(f.config());
        checkRejectedStart(engine);
        CHECK(engine.configure(timing));
        CHECK(engine.start());
        CHECK(engine.isRunning());
        CHECK(sdk::recording.smEnabled[0][0]);
        CHECK_EQ(sdk::recording.triggers, 1u);
        const auto calls = sdk::recording.calls;
        CHECK(engine.start());
        CHECK_EQ(sdk::recording.calls, calls);
        engine.stop(); engine.stop();
        CHECK(engine.start()); // an ordinary stop preserves a valid configuration
        CHECK_EQ(sdk::recording.triggers, 2u);
    }
    checkReleased();
}

PICOTTL_TEST(engineInvalidTimingNeverStarts) {
    sdk::reset(); Fixture f;
    HardwareScanlineEngine engine(f.config());
    auto invalid = timing;
    invalid.hSyncWidth = 2;
    CHECK(!engine.configure(invalid));
    checkRejectedStart(engine);
    engine.stop(); engine.stop();
    CHECK_EQ(sdk::recording.calls, 0u);
}

PICOTTL_TEST(engineRejectedReconfigurationInvalidatesAnIdleConfiguration) {
    sdk::reset(); Fixture f;
    HardwareScanlineEngine engine(f.config());
    CHECK(engine.configure(timing));
    auto invalid = timing;
    invalid.hSyncWidth = 2;
    CHECK(!engine.configure(invalid));
    checkQuiescent(3);
    checkRejectedStart(engine);
    CHECK(engine.configure(timing));
    CHECK(engine.start());
    CHECK(sdk::recording.dma[0].read == f.table);
}

PICOTTL_TEST(engineRejectedReconfigurationStopsActiveOutputAndCannotRestart) {
    sdk::reset(); Fixture f;
    HardwareScanlineEngine engine(f.config());
    CHECK(engine.configure(timing)); CHECK(engine.start());
    const auto originalSegment = f.table[0];
    auto invalid = timing;
    invalid.pixelClockHz = 200'000'000;
    invalid.hActive = 16;
    CHECK(!engine.configure(invalid));
    CHECK(f.table[0] != originalSegment); // failure does not roll the table back
    checkQuiescent(3);
    checkRejectedStart(engine);
    CHECK(engine.configure(timing)); CHECK(engine.start());
    CHECK_EQ(f.table[0], originalSegment);
}

PICOTTL_TEST(engineStoppedThenFailedConfigurationStaysInvalid) {
    sdk::reset(); Fixture f;
    {
        HardwareScanlineEngine engine(f.config());
        CHECK(engine.configure(timing)); CHECK(engine.start()); engine.stop();
        auto invalid = timing;
        invalid.vBackPorch = 50; // exceeds the caller's timing table capacity
        CHECK(!engine.configure(invalid));
        checkRejectedStart(engine);
        engine.stop(); engine.stop();
        checkQuiescent(3);
    }
    checkReleased();
}

PICOTTL_TEST(enginePixelShapeFailureInvalidatesAllOwnedChannels) {
    sdk::reset(); Fixture f;
    HardwareScanlineEngine engine(f.config());
    CHECK(engine.setPixelSource(f.pixels, 4));
    CHECK(engine.configure(timing));
    auto invalid = timing;
    invalid.hActive = 9; // non-byte-linear and wrong framebuffer size
    CHECK(!engine.configure(invalid));
    checkQuiescent(15);
    checkRejectedStart(engine);
}

PICOTTL_TEST(enginePixelClockFailureRecoversAndRearmsBothPairs) {
    sdk::reset(); Fixture f;
    {
        HardwareScanlineEngine engine(f.config());
        CHECK(engine.setPixelSource(f.pixels, 4));
        CHECK(engine.configure(timing));
        auto invalid = timing;
        invalid.pixelClockHz = 90'000'000; // timing divider valid; pixel divider < 1
        CHECK(!engine.configure(invalid));
        checkQuiescent(15);
        checkRejectedStart(engine);
        CHECK(engine.configure(timing));
        CHECK(engine.start());
        CHECK_EQ(sdk::recording.triggers, 2u);
        CHECK(sdk::recording.dma[2].read == f.pixels);
        CHECK(sdk::recording.dma[1].write == &dma_hw->ch[0].al3_read_addr_trig);
        CHECK(sdk::recording.dma[3].write == &dma_hw->ch[2].al3_read_addr_trig);
        for (uint ch = 0; ch < 4; ++ch) {
            CHECK((dma_hw->ch[ch].ctrl_trig & DMA_CH0_CTRL_TRIG_EN_BITS) != 0);
            CHECK(sdk::recording.dma[ch].configurations >= 3);
        }
        CHECK_EQ((dma_hw->ch[0].ctrl_trig & DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS) >> 13, 1u);
        CHECK_EQ((dma_hw->ch[2].ctrl_trig & DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS) >> 13, 3u);
        engine.stop();
        checkQuiescent(15);
    }
    checkReleased();
}

PICOTTL_TEST(engineInvalidHardwareParametersNeverAcquireResources) {
    for (unsigned failure = 0; failure < 7; ++failure) {
        sdk::reset(); Fixture f;
        auto config = f.config();
        switch (failure) {
            case 0: config.pioInstance = NUM_PIOS; break;
            case 1: config.stateMachine = NUM_PIO_STATE_MACHINES; break;
            case 2: config.dmaDataChannel = NUM_DMA_CHANNELS; break;
            case 3: config.dmaControlChannel = config.dmaDataChannel; break;
            case 4: config.hsyncPin = NUM_BANK0_GPIOS - 1; break;
            case 5: config.timingBuffer = nullptr; break;
            default: config.timingBufferWords = 0; break;
        }
        {
            HardwareScanlineEngine engine(config);
            CHECK(!engine.configure(timing));
            checkRejectedStart(engine);
        }
        CHECK_EQ(sdk::recording.calls, sdk::recording.clockReads);
        checkReleased();
    }
}

PICOTTL_TEST(enginePartialAcquisitionRespectsForeignOwnershipAndRecovers) {
    sdk::reset(); Fixture f;
    sdk::recording.dmaClaimed[3] = true; // blocks pixel acquisition only
    dma_hw->ch[3].ctrl_trig = DMA_CH0_CTRL_TRIG_EN_BITS;
    {
        HardwareScanlineEngine engine(f.config());
        CHECK(engine.setPixelSource(f.pixels, 4));
        CHECK(!engine.configure(timing));
        CHECK(sdk::recording.dmaClaimed[0]);
        CHECK(sdk::recording.dmaClaimed[1]);
        CHECK(!sdk::recording.dmaClaimed[2]);
        checkRejectedStart(engine);
        engine.stop(); engine.stop();
        CHECK(sdk::recording.dmaClaimed[3]);
        CHECK_EQ(dma_hw->ch[3].ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
        dma_channel_unclaim(3); // foreign owner explicitly releases its channel
        CHECK(engine.configure(timing)); CHECK(engine.start());
    }
    checkReleased();
}

PICOTTL_TEST(engineProgramAllocationFailureCanRetryEitherDepth) {
    for (unsigned depth = 1; depth <= 2; ++depth) {
        sdk::reset(); Fixture f;
        {
            HardwareScanlineEngine engine(f.config());
            CHECK(engine.setPixelSource(f.pixels, 4, 1));
            CHECK(engine.configure(timing));
            CHECK(engine.setPixelSource(f.pixels, 8, 2));
            sdk::recording.allowPixelProgram = false;
            CHECK(!engine.configure(timing));
            checkRejectedStart(engine);
            sdk::recording.allowPixelProgram = true;
            CHECK(engine.setPixelSource(f.pixels, 4 * depth, depth));
            CHECK(engine.configure(timing));
            CHECK(sdk::recording.programs[0][depth]);
            CHECK_EQ(sdk::recording.invalidPc, 0u);
            CHECK(engine.start());
            CHECK_EQ(sdk::recording.invalidPc, 0u);
        }
        checkReleased();
    }
}

PICOTTL_TEST(engineFailedConfigurationDestructionReleasesOnlyOwnedResources) {
    sdk::reset(); Fixture f;
    sdk::recording.dmaClaimed[9] = true;
    sdk::recording.smClaimed[0][2] = true;
    sdk::recording.smEnabled[0][2] = true;
    sdk::recording.allowPixelProgram = false;
    {
        HardwareScanlineEngine engine(f.config());
        CHECK(engine.setPixelSource(f.pixels, 4));
        CHECK(!engine.configure(timing));
        checkRejectedStart(engine);
        engine.stop(); engine.stop();
        checkQuiescent(15);
    }
    checkReleased();
    CHECK(sdk::recording.dmaClaimed[9]);
    CHECK(sdk::recording.smClaimed[0][2]);
    CHECK(sdk::recording.smEnabled[0][2]);
}

PICOTTL_TEST(engineTimingResourceFailuresAllowRetryWithoutStealingOwnership) {
    for (unsigned failure = 0; failure < 4; ++failure) {
        sdk::reset(); Fixture f;
        sdk::recording.allowTimingProgram = failure != 0;
        sdk::recording.smClaimed[0][0] = failure == 1;
        sdk::recording.dmaClaimed[0] = failure == 2;
        sdk::recording.dmaClaimed[1] = failure == 3;
        {
            HardwareScanlineEngine engine(f.config());
            CHECK(!engine.configure(timing));
            checkRejectedStart(engine);
            engine.stop(); engine.stop();
            CHECK_EQ(sdk::recording.releases, 0u);
            CHECK_EQ(sdk::recording.smClaimed[0][0], failure == 1);
            CHECK_EQ(sdk::recording.dmaClaimed[0], failure == 2);
            CHECK_EQ(sdk::recording.dmaClaimed[1], failure == 3);
            // The external owner makes its resources available.
            sdk::recording.allowTimingProgram = true;
            sdk::recording.smClaimed[0][0] = false;
            sdk::recording.dmaClaimed[0] = false;
            sdk::recording.dmaClaimed[1] = false;
            CHECK(engine.configure(timing)); CHECK(engine.start());
        }
        checkReleased();
    }
}

PICOTTL_TEST(engineFailedNewAcquisitionQuiescesPreexistingIdleRegisters) {
    sdk::reset(); Fixture f;
    // claim/unclaim changes SDK ownership only, not the register contents.
    // An idle, unclaimed channel can still have EN set; no transfers run here.
    for (uint ch = 0; ch < 4; ++ch) {
        dma_hw->ch[ch].ctrl_trig = DMA_CH0_CTRL_TRIG_EN_BITS |
            (ch << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB);
    }
    sdk::recording.allowPixelProgram = false;
    {
        HardwareScanlineEngine engine(f.config());
        CHECK(engine.setPixelSource(f.pixels, 4));
        CHECK(!engine.configure(timing));
        // Failure follows acquisition, before either DMA setup function.
        checkQuiescent(15);
        checkRejectedStart(engine);
        sdk::recording.allowPixelProgram = true;
        CHECK(engine.configure(timing)); CHECK(engine.start());
    }
    checkReleased();
}

PICOTTL_TEST(engineValidPixelDepthsAndDetachKeepTheirLifecycle) {
    for (unsigned depth = 1; depth <= 8; depth *= 2) {
        sdk::reset(); Fixture f;
        {
            HardwareScanlineEngine engine(f.config());
            CHECK(engine.setPixelSource(f.pixels, 4 * depth, depth, depth == 8 ? 6 : depth));
            CHECK(engine.configure(timing)); CHECK(engine.start());
            CHECK(sdk::recording.dma[2].read == f.pixels);
            CHECK_EQ(sdk::recording.dma[2].count, depth);
            CHECK(sdk::recording.smEnabled[0][0]);
            CHECK(sdk::recording.smEnabled[0][1]);
            engine.stop();
            CHECK(engine.setPixelSource(nullptr, 0));
            checkRejectedStart(engine);
            CHECK(engine.configure(timing)); CHECK(engine.start());
            CHECK(sdk::recording.smEnabled[0][0]);
            CHECK(!sdk::recording.smEnabled[0][1]);
            CHECK(!sdk::recording.dmaClaimed[9]);
        }
        checkReleased();
    }
}

namespace {
void checkClockRejectedBeforePio(HardwareScanlineEngine& engine) {
    CHECK_EQ(sdk::recording.calls, sdk::recording.clockReads);
    CHECK_EQ(sdk::recording.dividerCalls, 0u);
    CHECK_EQ(sdk::recording.integerDividerCalls, 0u);
    CHECK_EQ(sdk::recording.gpioConfigurations, 0u);
    CHECK_EQ(sdk::recording.invalidDividerCalls, 0u);
    for (uint ch = 0; ch < 4; ++ch) {
        CHECK(!sdk::recording.dmaClaimed[ch]);
        CHECK_EQ(sdk::recording.dma[ch].configurations, 0u);
    }
    checkRejectedStart(engine);
}
} // namespace

PICOTTL_TEST(engineRejectsZeroReportedAndRequestedClocks) {
    const std::uint32_t clocks[][2] = {{0, 10'000'000}, {130'000'000, 0}, {0, 0}};
    for (const auto& pair : clocks) {
        sdk::reset(); Fixture f;
        sdk::recording.clockHz = pair[0];
        HardwareScanlineEngine engine(f.config());
        auto requested = timing; requested.pixelClockHz = pair[1];
        CHECK(!engine.configure(requested));
        checkClockRejectedBeforePio(engine);
    }
}

PICOTTL_TEST(engineRejectsUnsupportedClockRatiosBeforePioSetup) {
    const std::uint32_t clocks[][2] = {
        {99, 100}, {1, UINT32_MAX}, {65'537, 1}, {UINT32_MAX, 1},
        // A float conversion/division can round these out-of-range ratios
        // back onto 1 or 65536. Validate the integer frequencies exactly.
        {16'777'216, 16'777'217}, {65'536'001, 1000}
    };
    for (const auto& pair : clocks) {
        sdk::reset(); Fixture f;
        sdk::recording.clockHz = pair[0];
        HardwareScanlineEngine engine(f.config());
        auto requested = timing; requested.pixelClockHz = pair[1];
        CHECK(!engine.configure(requested));
        checkClockRejectedBeforePio(engine);
    }
}

PICOTTL_TEST(engineAcceptsClockDividerEndpointsWithoutUnsafeConversion) {
    struct Case { std::uint32_t system, pixel; float expected; };
    const Case cases[] = {
        {1000, 1000, 1}, {UINT32_MAX, UINT32_MAX, 1},
        {65'535, 1, 65535}, {65'536, 1, 65536},
        {65'536'000, 1000, 65536}, {UINT32_MAX, 65'536, 65536}
    };
    for (const auto& item : cases) {
        sdk::reset(); Fixture f;
        sdk::recording.clockHz = item.system;
        HardwareScanlineEngine engine(f.config());
        auto requested = timing; requested.pixelClockHz = item.pixel;
        CHECK(engine.configure(requested));
        CHECK(sdk::recording.dividers[0][0] == item.expected);
        CHECK_EQ(sdk::recording.fullPeriodEncoded[0][0], item.expected == 65536);
        CHECK_EQ(sdk::recording.invalidDividerCalls, 0u);
        CHECK_EQ(sdk::recording.floatEndpointCalls, 0u);
        CHECK(engine.start());
    }
}

PICOTTL_TEST(enginePreservesFractionalDividersAndIntegerSnapPolicy) {
    struct Case { std::uint32_t system, pixel; float expected; };
    const Case cases[] = {
        {1500, 1000, 1.5f}, {2001, 1000, 2},
        {538, 256, 2.1015625f}, {1077, 512, 2.103515625f},
        {101, 100, 1}, {102, 100, 1.02f},
        {16'777'215, 256, 65536} // largest float below 65536, then integer snap
    };
    for (const auto& item : cases) {
        sdk::reset(); Fixture f;
        sdk::recording.clockHz = item.system;
        HardwareScanlineEngine engine(f.config());
        auto requested = timing; requested.pixelClockHz = item.pixel;
        CHECK(engine.configure(requested));
        CHECK(sdk::recording.dividers[0][0] == item.expected);
        CHECK_EQ(sdk::recording.invalidDividerCalls, 0u);
        CHECK_EQ(sdk::recording.floatEndpointCalls, 0u);
    }
}

PICOTTL_TEST(engineValidatesPixelClockAfterIntegerSnapBeforePioSetup) {
    struct Case { std::uint32_t system, pixel; float sync, pixels; bool valid; };
    const Case cases[] = {
        {198, 100, 2, 1, true}, {200, 100, 2, 1, true},
        {197, 100, 0, 0, false}, {1500, 1000, 0, 0, false},
        {65'536, 1, 65536, 32768, true}
    };
    for (const auto& item : cases) {
        sdk::reset(); Fixture f;
        sdk::recording.clockHz = item.system;
        HardwareScanlineEngine engine(f.config());
        CHECK(engine.setPixelSource(f.pixels, 4));
        auto requested = timing; requested.pixelClockHz = item.pixel;
        CHECK_EQ(engine.configure(requested), item.valid);
        if (item.valid) {
            CHECK(sdk::recording.dividers[0][0] == item.sync);
            CHECK(sdk::recording.dividers[0][1] == item.pixels);
            CHECK_EQ(sdk::recording.invalidDividerCalls, 0u);
            CHECK_EQ(sdk::recording.floatEndpointCalls, 0u);
            CHECK(engine.start());
        } else {
            checkClockRejectedBeforePio(engine);
        }
    }
}

PICOTTL_TEST(engineClockRejectionStopsOutputAndAllowsRecovery) {
    sdk::reset(); Fixture f;
    HardwareScanlineEngine engine(f.config());
    CHECK(engine.setPixelSource(f.pixels, 4));
    CHECK(engine.configure(timing)); CHECK(engine.start());
    for (unsigned failure = 0; failure < 3; ++failure) {
        const auto dividerCalls = sdk::recording.dividerCalls;
        const auto gpioConfigurations = sdk::recording.gpioConfigurations;
        auto invalid = timing;
        sdk::recording.clockHz = failure == 0 ? 0 : 130'000'000;
        invalid.pixelClockHz = failure == 1 ? 0 : (failure == 2 ? 1 : timing.pixelClockHz);
        CHECK(!engine.configure(invalid));
        checkQuiescent(15);
        checkRejectedStart(engine);
        CHECK_EQ(sdk::recording.dividerCalls, dividerCalls);
        CHECK_EQ(sdk::recording.gpioConfigurations, gpioConfigurations);
        sdk::recording.clockHz = 130'000'000;
        CHECK(engine.configure(timing)); CHECK(engine.start());
        CHECK(sdk::recording.dma[2].read == f.pixels);
    }
    CHECK_EQ(sdk::recording.invalidDividerCalls, 0u);
    CHECK_EQ(sdk::recording.floatEndpointCalls, 0u);
}

PICOTTL_TEST(engineRejectsEmptyFramesAndRecoversWithoutRejectingZeroReportedRate) {
    sdk::reset(); Fixture f;
    HardwareScanlineEngine engine(f.config());
    auto empty = timing;
    empty.vActive = empty.vFrontPorch = empty.vSyncWidth = empty.vBackPorch = 0;
    CHECK(!engine.configure(empty));
    CHECK_EQ(sdk::recording.calls, 0u); // reject before acquiring hardware
    checkRejectedStart(engine);
    CHECK(engine.configure(timing)); CHECK(engine.start());
    const auto dividerCalls = sdk::recording.dividerCalls;
    CHECK(!engine.configure(empty));
    CHECK_EQ(sdk::recording.dividerCalls, dividerCalls);
    checkQuiescent(3);
    checkRejectedStart(engine);
    engine.stop(); engine.stop();

    auto slow = timing;
    slow.pixelClockHz = 2000;
    slow.hActive = slow.hFrontPorch = slow.hSyncWidth = slow.hBackPorch = UINT16_MAX;
    slow.vActive = 11; // 14 lines, within the fixture's timing-table capacity.
    CHECK_EQ(slow.lineFrequencyHz(), 0u);
    CHECK_EQ(slow.refreshRateMilliHz(), 0u);
    CHECK(engine.configure(slow)); CHECK(engine.start());
    engine.stop();
    CHECK(engine.configure(timing)); CHECK(engine.start());
}

PICOTTL_TEST(engineStandardModeClockDividersRemainUnchanged) {
    struct Case { const VideoTiming* mode; std::uint32_t system; unsigned depth; float expected; };
    const Case cases[] = {
        {&picottl::modes::kIbmMda, 130'000'000, 2, 8},
        {&picottl::modes::kMdaOverscan, 130'000'000, 2, 8},
        {&picottl::modes::kIbmCga640, 142'800'000, 4, 10},
        {&picottl::modes::kIbmCga320, 142'800'000, 4, 20},
        {&picottl::modes::kIbmEga350, 130'000'000, 8, 8},
        // The default Pico 2 system clock also preserves the existing
        // fractional dividers, except CGA320 which snaps to 21.
        {&picottl::modes::kIbmMda, 150'000'000, 2, 150'000'000.0f / 16'257'000.0f},
        {&picottl::modes::kMdaOverscan, 150'000'000, 2, 150'000'000.0f / 16'257'000.0f},
        {&picottl::modes::kIbmCga640, 150'000'000, 4, 150'000'000.0f / 14'318'180.0f},
        {&picottl::modes::kIbmCga320, 150'000'000, 4, 21},
        {&picottl::modes::kIbmEga350, 150'000'000, 8, 150'000'000.0f / 16'257'000.0f}
    };
    alignas(std::uint32_t) static std::uint8_t pixels[224'000];
    std::uint32_t table[2048];
    for (const auto& item : cases) {
        sdk::reset();
        Fixture f;
        auto config = f.config(); config.timingBuffer = table; config.timingBufferWords = 2048;
        HardwareScanlineEngine engine(config);
        sdk::recording.clockHz = item.system;
        const auto bytes = static_cast<std::size_t>(item.mode->hActive) * item.mode->vActive * item.depth / 8;
        CHECK(engine.setPixelSource(pixels, bytes, item.depth, item.depth == 8 ? 6 : item.depth));
        CHECK(engine.configure(*item.mode)); CHECK(engine.start());
        CHECK(sdk::recording.dividers[0][0] == item.expected);
        CHECK(sdk::recording.dividers[0][1] == item.expected / 2);
        CHECK_EQ(sdk::recording.invalidDividerCalls, 0u);
        CHECK_EQ(sdk::recording.floatEndpointCalls, 0u);
    }
}

namespace {
void checkNoPinChanges() {
    CHECK_EQ(sdk::recording.gpioConfigurations, 0u);
    CHECK_EQ(sdk::recording.pinDirections, 0u);
    CHECK_EQ(sdk::recording.pinWrites, 0u);
    CHECK_EQ(sdk::recording.padConfigurations, 0u);
}
}

PICOTTL_TEST(engineInvalidPixelParametersHaveNoHardwareSideEffects) {
    for (unsigned failure = 0; failure < 7; ++failure) {
        sdk::reset(); Fixture f;
        auto config = f.config();
        switch (failure) {
            case 0: config.pixelStateMachine = config.stateMachine; break;
            case 1: config.pixelDmaDataChannel = NUM_DMA_CHANNELS; break;
            case 2: config.pixelDmaControlChannel = config.pixelDmaDataChannel; break;
            case 3: config.pixelDmaDataChannel = config.dmaControlChannel; break;
            case 4: config.hsyncPin = NUM_BANK0_GPIOS - 2; break;
            case 5: config.videoPin = config.hsyncPin; break;
            default: config.videoPin = NUM_BANK0_GPIOS; break;
        }
        {
            HardwareScanlineEngine engine(config);
            CHECK(engine.setPixelSource(f.pixels, 4));
            for (unsigned repeat = 0; repeat < 3; ++repeat) {
                CHECK(!engine.configure(timing));
                checkRejectedStart(engine);
                checkNoPinChanges();
                checkReleased();
            }
        }
        checkReleased();
    }
}

PICOTTL_TEST(enginePixelAcquisitionFailuresDoNotTouchPinsAndCanRetry) {
    for (unsigned failure = 0; failure < 4; ++failure) {
        sdk::reset(); Fixture f;
        sdk::recording.smClaimed[0][1] = failure == 0;
        sdk::recording.dmaClaimed[2] = failure == 1;
        sdk::recording.dmaClaimed[3] = failure == 2;
        sdk::recording.allowPixelProgram = failure != 3;
        {
            HardwareScanlineEngine engine(f.config());
            CHECK(engine.setPixelSource(f.pixels, 4));
            for (unsigned repeat = 0; repeat < 3; ++repeat) {
                CHECK(!engine.configure(timing));
                checkRejectedStart(engine);
                checkNoPinChanges();
                CHECK(sdk::recording.smClaimed[0][0]); // retained owner, not a leak
                CHECK(sdk::recording.dmaClaimed[0]);
                CHECK(sdk::recording.dmaClaimed[1]);
                CHECK_EQ(sdk::recording.smClaimed[0][1], failure == 0 || failure == 3);
                CHECK_EQ(sdk::recording.dmaClaimed[2], failure == 1 || failure == 3);
                CHECK_EQ(sdk::recording.dmaClaimed[3], failure == 2 || failure == 3);
                checkQuiescent(3);
            }
            sdk::recording.smClaimed[0][1] = failure == 3;
            sdk::recording.dmaClaimed[2] = failure == 3;
            sdk::recording.dmaClaimed[3] = failure == 3;
            sdk::recording.allowPixelProgram = true;
            CHECK(engine.configure(timing)); CHECK(engine.start());
            engine.stop(); CHECK(engine.start());
            CHECK_EQ((dma_hw->ch[0].ctrl_trig & DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS) >> 13, 1u);
            CHECK_EQ((dma_hw->ch[2].ctrl_trig & DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS) >> 13, 3u);
        }
        checkReleased();
    }
}

PICOTTL_TEST(engineSdkProgramAddErrorsNeverBecomeConfigurations) {
    for (unsigned failure = 0; failure < 2; ++failure) {
        sdk::reset(); Fixture f;
        sdk::recording.failTimingProgramAdd = failure == 0;
        sdk::recording.failPixelProgramAdd = failure == 1;
        {
            HardwareScanlineEngine engine(f.config());
            CHECK(engine.setPixelSource(f.pixels, 4));
            CHECK(!engine.configure(timing));
            checkRejectedStart(engine);
            checkNoPinChanges();
            CHECK_EQ(sdk::recording.invalidPc, 0u);
            CHECK(!sdk::recording.programs[0][1]);
            sdk::recording.failTimingProgramAdd = false;
            sdk::recording.failPixelProgramAdd = false;
            CHECK(engine.configure(timing)); CHECK(engine.start());
        }
        checkReleased();
    }
}

PICOTTL_TEST(engineFailedDepthReconfigurationPreservesPinsAndOwnedResources) {
    for (bool addError : {false, true}) {
        sdk::reset(); Fixture f;
        sdk::recording.smClaimed[0][2] = true;
        sdk::recording.smEnabled[0][2] = true;
        sdk::recording.dmaClaimed[9] = true;
        dma_hw->ch[9].ctrl_trig = DMA_CH0_CTRL_TRIG_EN_BITS;
        {
            HardwareScanlineEngine engine(f.config());
            CHECK(engine.setPixelSource(f.pixels, 4));
            CHECK(engine.configure(timing)); CHECK(engine.start()); engine.stop();
            const auto mux = sdk::recording.gpioConfigurations;
            const auto directions = sdk::recording.pinDirections;
            const auto writes = sdk::recording.pinWrites;
            const auto pads = sdk::recording.padConfigurations;
            const auto claims = sdk::recording.dmaClaimed;
            const auto stateMachines = sdk::recording.smClaimed;
            CHECK(engine.setPixelSource(f.pixels, 8, 2));
            sdk::recording.allowPixelProgram = addError;
            sdk::recording.failPixelProgramAdd = addError;
            for (unsigned repeat = 0; repeat < 3; ++repeat) {
                CHECK(!engine.configure(timing));
                checkRejectedStart(engine);
                checkQuiescent(15);
                CHECK(sdk::recording.dmaClaimed == claims);
                CHECK(sdk::recording.smClaimed == stateMachines);
                CHECK_EQ(sdk::recording.gpioConfigurations, mux);
                CHECK_EQ(sdk::recording.pinDirections, directions);
                CHECK_EQ(sdk::recording.pinWrites, writes);
                CHECK_EQ(sdk::recording.padConfigurations, pads);
                CHECK(sdk::recording.smEnabled[0][2]);
                CHECK_EQ(dma_hw->ch[9].ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
            }
            sdk::recording.allowPixelProgram = true;
            sdk::recording.failPixelProgramAdd = false;
            CHECK(engine.configure(timing)); CHECK(engine.start());
            CHECK(sdk::recording.programs[0][2]);
            CHECK_EQ(sdk::recording.dma[2].count, 2u);
            engine.stop(); CHECK(engine.start());
            CHECK_EQ((dma_hw->ch[2].ctrl_trig & DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS) >> 13, 3u);
        }
        checkReleased();
        CHECK(sdk::recording.smClaimed[0][2]);
        CHECK(sdk::recording.smEnabled[0][2]);
        CHECK(sdk::recording.dmaClaimed[9]);
        CHECK_EQ(dma_hw->ch[9].ctrl_trig, DMA_CH0_CTRL_TRIG_EN_BITS);
    }
}

PICOTTL_TEST(engineInvalidPixelConfigurationDoesNotBlockAnotherEngine) {
    sdk::reset(); Fixture f;
    auto bad = f.config(); bad.videoPin = bad.hsyncPin;
    HardwareScanlineEngine rejected(bad);
    CHECK(rejected.setPixelSource(f.pixels, 4));
    CHECK(!rejected.configure(timing));
    checkReleased(); checkNoPinChanges();
    {
        HardwareScanlineEngine valid(f.config());
        CHECK(valid.setPixelSource(f.pixels, 4));
        CHECK(valid.configure(timing)); CHECK(valid.start());
        rejected.stop();
        CHECK(valid.isRunning());
        CHECK(sdk::recording.smEnabled[0][0]);
        CHECK((dma_hw->ch[0].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS) != 0);
    }
    checkReleased();
}

PICOTTL_TEST(engineAPackageRejectsUnavailablePinsBeforeGpioSetup) {
    for (unsigned video : {29u, 30u, 31u, 32u, 34u, 47u}) {
        sdk::reset(); Fixture f;
        auto config = f.config(); config.videoPin = static_cast<std::uint8_t>(video);
        {
            HardwareScanlineEngine engine(config);
            CHECK(engine.setPixelSource(f.pixels, 4));
            CHECK_EQ(engine.configure(timing), video == 29);
            if (video == 29) { CHECK(engine.start()); engine.stop(); }
            else { checkRejectedStart(engine); checkNoPinChanges(); checkReleased(); }
            CHECK_EQ(sdk::recording.invalidPinCalls, 0u);
        }
        checkReleased();
    }
}
