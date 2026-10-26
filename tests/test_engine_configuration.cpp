// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#include "support/HardwareScanlineEngine.hpp"
#include "support/pico_sdk/RecordingPicoSdk.hpp"
#include "picottl/video/ModeTimings.hpp"
#include <vector>

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

std::uint32_t divider256(unsigned sm) {
    const auto raw = sdk::recording.clkdivRegisters[0][sm];
    return raw == 0 ? 65'536u * 256u : raw >> 8;
}

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
    struct Case { std::uint32_t system, pixel; std::uint32_t expected256; };
    const Case cases[] = {
        {1500, 1000, 384}, {2001, 1000, 512},
        {538, 256, 538}, {1077, 512, 539},
        {101, 100, 256}, {102, 100, 261},
        {16'777'215, 256, 16'777'216} // largest float below 65536, then integer snap
    };
    for (const auto& item : cases) {
        sdk::reset(); Fixture f;
        sdk::recording.clockHz = item.system;
        HardwareScanlineEngine engine(f.config());
        auto requested = timing; requested.pixelClockHz = item.pixel;
        CHECK(engine.configure(requested));
        CHECK_EQ(divider256(0), item.expected256);
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
    struct Case { const VideoTiming* mode; std::uint32_t system; unsigned depth; std::uint32_t expected256; };
    const Case cases[] = {
        {&picottl::modes::kIbmMda, 130'000'000, 2, 2048},
        {&picottl::modes::kMdaOverscan, 130'000'000, 2, 2048},
        {&picottl::modes::kIbmCga640, 142'800'000, 4, 2560},
        {&picottl::modes::kIbmCga320, 142'800'000, 4, 5120},
        {&picottl::modes::kIbmEga350, 130'000'000, 8, 2048},
        // Exact SDK-default encoded dividers, including the existing
        // fractional values at 150 MHz and CGA320's integer snap to 21.
        {&picottl::modes::kIbmMda, 150'000'000, 2, 2362},
        {&picottl::modes::kMdaOverscan, 150'000'000, 2, 2362},
        {&picottl::modes::kIbmCga640, 150'000'000, 4, 2682},
        {&picottl::modes::kIbmCga320, 150'000'000, 4, 5376},
        {&picottl::modes::kIbmEga350, 150'000'000, 8, 2362}
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
        CHECK_EQ(divider256(0), item.expected256);
        CHECK_EQ(divider256(1), item.expected256 / 2u);
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

PICOTTL_TEST(engineCgaClockPairUsesOneSdkQuantization) {
    struct Case { std::uint32_t system, timing256, pixel256; bool nearest; };
    const Case cases[] = {
        {150'000'000, 2682, 1341, true}, // default SDK: original example is already coherent
        {150'000'000, 2680, 1340, false}, // original audit's truncating configuration
        {125'000'000, 2234, 1117, true},
        {125'000'000, 2234, 1117, false},
        {133'000'000, 2378, 1189, true},
        {133'000'000, 2376, 1188, false}
    };
    alignas(4) static std::uint8_t pixels[64'000];
    std::uint32_t table[2048];
    for (const auto& c : cases) {
        sdk::reset(); Fixture f; auto config = f.config();
        config.timingBuffer = table; config.timingBufferWords = 2048;
        sdk::recording.clockHz = c.system;
        sdk::recording.roundDividerNearest = c.nearest;
        HardwareScanlineEngine engine(config);
        CHECK(engine.setPixelSource(pixels, 64'000, 4));
        CHECK(engine.configure(picottl::modes::kIbmCga640));
        CHECK_EQ(divider256(0), c.timing256); CHECK_EQ(divider256(1), c.pixel256);
        CHECK_EQ(divider256(0), 2u * divider256(1));
        CHECK_EQ(sdk::recording.floatEndpointCalls, 0u);
        CHECK(engine.start());
    }
}

PICOTTL_TEST(engineModeClockPairsStayCoherentAcrossClocksAndDepths) {
    struct Mode { const VideoTiming* timing; unsigned maxDepth; };
    const Mode modes[] = {{&picottl::modes::kIbmMda, 2}, {&picottl::modes::kMdaOverscan, 2},
        {&picottl::modes::kIbmCga640, 4}, {&picottl::modes::kIbmCga320, 4},
        {&picottl::modes::kIbmEga350, 8}};
    // 200/250 MHz are PLL-achievable overclock scenarios, not rated operation.
    const std::uint32_t clocks[] = {125'000'000, 130'000'000, 133'000'000,
        150'000'000, 200'000'000, 250'000'000};
    alignas(4) static std::uint8_t pixels[224'000];
    std::uint32_t table[2048];
    for (bool nearest : {false, true}) for (auto system : clocks) for (const auto& mode : modes) {
        for (unsigned depth = 1; depth <= mode.maxDepth; depth *= 2) {
            sdk::reset(); Fixture f; auto config = f.config();
            config.timingBuffer = table; config.timingBufferWords = 2048;
            sdk::recording.clockHz = system; sdk::recording.roundDividerNearest = nearest;
            HardwareScanlineEngine engine(config);
            const auto bytes = static_cast<std::size_t>(mode.timing->hActive) * mode.timing->vActive * depth / 8u;
            CHECK(engine.setPixelSource(pixels, bytes, static_cast<std::uint8_t>(depth), depth == 8 ? 6 : depth));
            CHECK(engine.configure(*mode.timing));
            const auto t = divider256(0), p = divider256(1);
            CHECK_EQ(t, 2u * p); CHECK(p >= 256); CHECK(t <= 65'536u * 256u);
            const auto nominal = static_cast<std::uint64_t>(mode.timing->pixelClockHz) * t;
            const auto effective = static_cast<std::uint64_t>(system) * 256u;
            const auto error = nominal > effective ? nominal - effective : effective - nominal;
            CHECK(error * 100u <= nominal); // Existing 1% snapping envelope, checked exactly.
            CHECK(engine.start()); engine.stop(); CHECK(engine.start()); engine.stop();
            CHECK_EQ(sdk::recording.abortWhileEnabled, 0u);
        }
    }
}

PICOTTL_TEST(engineCoherentClockBoundariesRejectBeforeHardwareAndRecover) {
    struct Case { std::uint32_t system, pixel, t, p; bool valid; };
    const Case cases[] = {
        {1000, 1000, 0, 0, false}, {1500, 1000, 0, 0, false},
        {197, 100, 0, 0, false}, {198, 100, 512, 256, true},
        {200, 100, 512, 256, true}, {517, 256, 512, 256, true},
        {518, 256, 518, 259, true}, {539, 256, 540, 270, true},
        {65'536, 1, 16'777'216, 8'388'608, true},
        {UINT32_MAX, 65'536, 16'777'216, 8'388'608, true},
        {65'537, 1, 0, 0, false}, {UINT32_MAX, 1, 0, 0, false},
        {0, 1000, 0, 0, false}, {1000, 0, 0, 0, false}
    };
    for (const auto& c : cases) {
        sdk::reset(); Fixture f; HardwareScanlineEngine engine(f.config());
        sdk::recording.clockHz = c.system;
        CHECK(engine.setPixelSource(f.pixels, 4));
        auto requested = timing; requested.pixelClockHz = c.pixel;
        CHECK_EQ(engine.configure(requested), c.valid);
        if (c.valid) {
            CHECK_EQ(divider256(0), c.t); CHECK_EQ(divider256(1), c.p);
            CHECK_EQ(divider256(0), 2u * divider256(1));
            if (c.t == 16'777'216) CHECK_EQ(sdk::recording.clkdivRegisters[0][0], 0u);
            CHECK_EQ(sdk::recording.invalidDividerCalls, 0u);
            CHECK_EQ(sdk::recording.floatEndpointCalls, 0u);
        } else {
            checkClockRejectedBeforePio(engine);
            sdk::recording.clockHz = 130'000'000;
            CHECK(engine.configure(timing)); CHECK_EQ(divider256(0), 2u * divider256(1));
            CHECK(engine.start());
        }
    }
}

PICOTTL_TEST(engineSignalOnlyKeepsFullDividerResolution) {
    sdk::reset(); Fixture f; HardwareScanlineEngine engine(f.config());
    sdk::recording.clockHz = 539;
    auto requested = timing; requested.pixelClockHz = 256;
    CHECK(engine.configure(requested)); CHECK_EQ(divider256(0), 539u);
    CHECK(!sdk::recording.smClaimed[0][1]);
    CHECK(engine.setPixelSource(f.pixels, 4)); CHECK(engine.configure(requested));
    CHECK_EQ(divider256(0), 540u); CHECK_EQ(divider256(1), 270u);
    CHECK(engine.start()); engine.stop();
    CHECK(engine.setPixelSource(nullptr, 0)); CHECK(engine.configure(requested));
    CHECK_EQ(divider256(0), 539u); CHECK(engine.start());
    CHECK(!sdk::recording.smEnabled[0][1]);
}

PICOTTL_TEST(engineCoherentClockRecoveryPreservesStopStartChaining) {
    sdk::reset(); Fixture f; HardwareScanlineEngine engine(f.config());
    sdk::recording.clockHz = 539;
    auto requested = timing; requested.pixelClockHz = 256;
    CHECK(engine.setPixelSource(f.pixels, 4)); CHECK(engine.configure(requested));
    CHECK_EQ(divider256(0), 540u); CHECK_EQ(divider256(1), 270u);
    CHECK(engine.start()); engine.stop(); CHECK(engine.start());
    const auto dividers = sdk::recording.clkdivRegisters;
    const auto gpio = sdk::recording.gpioConfigurations;
    sdk::recording.clockHz = 0;
    CHECK(!engine.configure(requested)); checkRejectedStart(engine); checkQuiescent(15);
    CHECK(sdk::recording.clkdivRegisters == dividers);
    CHECK_EQ(sdk::recording.gpioConfigurations, gpio);
    sdk::recording.clockHz = 539;
    CHECK(engine.configure(requested)); CHECK(engine.start());
    CHECK_EQ(divider256(0), 540u); CHECK_EQ(divider256(1), 270u);
    CHECK_EQ((dma_hw->ch[0].ctrl_trig & DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS) >> 13, 1u);
    CHECK_EQ((dma_hw->ch[2].ctrl_trig & DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS) >> 13, 3u);
}

PICOTTL_TEST(engineRejectsUnknownSyncPolaritiesBeforeHardware) {
    for (bool pixels : {false, true}) for (bool vertical : {false, true}) for (unsigned value : {2u, 255u}) {
        sdk::reset(); Fixture f;
        for (auto& word : f.table) word = 0xdeadbeefu;
        HardwareScanlineEngine engine(f.config());
        if (pixels) CHECK(engine.setPixelSource(f.pixels, 4));
        auto invalid = timing;
        (vertical ? invalid.vSyncPolarity : invalid.hSyncPolarity) = static_cast<picottl::SyncPolarity>(value);
        CHECK(!engine.configure(invalid)); checkRejectedStart(engine);
        checkNoPinChanges(); checkReleased(); CHECK_EQ(sdk::recording.calls, 0u);
        for (auto word : f.table) CHECK_EQ(word, 0xdeadbeefu);
        CHECK(engine.configure(timing)); CHECK(engine.start()); engine.stop();
    }
}

PICOTTL_TEST(engineRejectsPixelCountsThatWouldSelectAnotherDmaMode) {
    struct Case { std::uint16_t width, height; std::uint8_t depth; };
    const Case cases[] = {{32768, 32768, 8}, {65532, 32772, 4}};
    for (const auto& c : cases) {
        sdk::reset(); Fixture f;
        auto requested = timing;
        requested.hActive = c.width; requested.hFrontPorch = 0;
        requested.hSyncWidth = requested.hBackPorch = 3;
        requested.vActive = c.height; requested.vFrontPorch = requested.vBackPorch = 0; requested.vSyncWidth = 1;
        const auto required = HardwareScanlineEngine::timingWordsFor(requested);
        std::vector<std::uint32_t> table(required + 2u, 0xdeadbeefu);
        const auto untouched = table;
        auto config = f.config(); config.timingBuffer = table.data() + 1; config.timingBufferWords = required;
        {
            HardwareScanlineEngine engine(config);
            const auto bytes = static_cast<std::size_t>(c.width) * c.height * c.depth / 8u;
            // Only metadata is inspected; never start DMA for these giant source lengths.
            CHECK(engine.setPixelSource(f.pixels, bytes, c.depth));
            const bool accepted = engine.configure(requested);
            CHECK(!accepted); CHECK(!engine.isRunning());
            if (!accepted) checkRejectedStart(engine);
            checkNoPinChanges(); checkReleased(); CHECK_EQ(sdk::recording.calls, 0u);
            CHECK(table == untouched);
            CHECK_EQ(sdk::recording.dma[2].count & DMA_CH0_TRANS_COUNT_MODE_BITS, 0u);
            CHECK_EQ(sdk::recording.triggers, 0u);
            CHECK(engine.setPixelSource(f.pixels, 4));
            CHECK(engine.configure(timing)); CHECK(engine.start());
        }
        checkReleased();
    }
}

PICOTTL_TEST(engineNormalPixelDmaCountBoundaryRemainsRepresentable) {
    struct Case { std::uint16_t height; std::uint8_t depth; std::uint32_t words; };
    const Case cases[] = {{16384, 8, 0x0fffc000u}, {16385, 8, 0x0fffffffu},
        {32768, 4, 0x0fffc000u}, {32770, 4, 0x0fffffffu}};
    for (const auto& c : cases) {
        sdk::reset(); Fixture f; auto requested = timing;
        requested.hActive = 65532; requested.hFrontPorch = 0;
        requested.hSyncWidth = requested.hBackPorch = 3;
        requested.vActive = c.height; requested.vFrontPorch = requested.vBackPorch = 0; requested.vSyncWidth = 1;
        const auto required = HardwareScanlineEngine::timingWordsFor(requested);
        std::vector<std::uint32_t> table(required + 2u, 0xdeadbeefu);
        auto config = f.config(); config.timingBuffer = table.data() + 1; config.timingBufferWords = required;
        {
            HardwareScanlineEngine engine(config);
            // This is a count/encoding test, not a physically backed gigabyte framebuffer.
            // configure() must not trigger or read pixel data; start() is deliberately omitted.
            const auto bytes = static_cast<std::size_t>(requested.hActive) * c.height * c.depth / 8u;
            CHECK(engine.setPixelSource(f.pixels, bytes, c.depth)); CHECK(engine.configure(requested));
            CHECK_EQ(sdk::recording.dma[2].count, c.words);
            CHECK_EQ(sdk::recording.dma[2].count & DMA_CH0_TRANS_COUNT_MODE_BITS, 0u);
            CHECK_EQ(sdk::recording.dma[0].count, required);
            CHECK_EQ(sdk::recording.triggers, 0u);
            CHECK_EQ(table.front(), 0xdeadbeefu); CHECK_EQ(table.back(), 0xdeadbeefu);
            engine.stop(); engine.stop(); checkQuiescent(15);
        }
        checkReleased();
    }
}

PICOTTL_TEST(engineZeroVerticalIntervalsRemainExecutableInSignalOnlyMode) {
    const std::uint16_t phases[][4] = {{1,0,0,0}, {0,0,1,0}, {0,1,0,0}, {0,0,0,1}, {1,0,1,0}, {0,1,1,1}, {0,0,0,0}};
    for (const auto& phase : phases) {
        sdk::reset(); Fixture f; auto requested = timing;
        requested.hActive = requested.hSyncWidth = requested.hBackPorch = 3; requested.hFrontPorch = 0;
        requested.vActive = phase[0]; requested.vFrontPorch = phase[1]; requested.vSyncWidth = phase[2]; requested.vBackPorch = phase[3];
        HardwareScanlineEngine engine(f.config());
        const bool nonempty = requested.vTotal() != 0;
        CHECK_EQ(engine.configure(requested), nonempty);
        if (!nonempty) { checkRejectedStart(engine); checkNoPinChanges(); checkReleased(); continue; }
        CHECK_EQ(sdk::recording.dma[0].count, requested.vTotal() * 3u);
        for (unsigned line = 0; line < requested.vTotal(); ++line) {
            const bool sync = line >= requested.vActive + requested.vFrontPorch &&
                line < requested.vActive + requested.vFrontPorch + requested.vSyncWidth;
            const unsigned vs = sync ? 0u : 2u; // active-low VSYNC
            CHECK_EQ(f.table[line * 3u], vs | (line < requested.vActive ? 4u : 0u));
            CHECK_EQ(f.table[line * 3u + 1u], vs | 1u);
            CHECK_EQ(f.table[line * 3u + 2u], vs);
        }
        CHECK(engine.start()); engine.stop(); CHECK(!sdk::recording.smClaimed[0][1]);
    }
    // Minimum byte/word packing differs with depth; no positive source can cover zero active lines.
    struct PixelCase { std::uint16_t width, height; std::uint8_t depth; std::size_t bytes; };
    const PixelCase cases[] = {{8,4,1,4}, {4,4,2,4}, {4,2,4,4}, {3,4,8,12}, {4,1,8,4}};
    for (const auto& c : cases) {
        sdk::reset(); Fixture f; auto requested = timing;
        requested.hActive = c.width; requested.hFrontPorch = 0; requested.hSyncWidth = requested.hBackPorch = 3;
        requested.vActive = c.height; requested.vFrontPorch = requested.vSyncWidth = requested.vBackPorch = 0;
        HardwareScanlineEngine engine(f.config());
        CHECK(engine.setPixelSource(f.pixels, c.bytes, c.depth));
        auto empty = requested; empty.vActive = 0; empty.vSyncWidth = 1;
        CHECK(!engine.configure(empty)); checkNoPinChanges(); checkReleased();
        CHECK(engine.configure(requested)); CHECK(engine.start()); engine.stop();
        CHECK_EQ(divider256(0), 2u * divider256(1));
    }
}

PICOTTL_TEST(engineHorizontalSegmentBoundariesPreserveEncoding) {
    struct Case { unsigned field; std::uint16_t width; bool valid; };
    const Case cases[] = {{0,0,false},{0,1,false},{0,2,false},{0,3,true},{0,UINT16_MAX,true},
        {1,0,true},{1,1,false},{1,2,false},{1,3,true},{1,UINT16_MAX,true},
        {2,0,false},{2,1,false},{2,2,false},{2,3,true},{2,UINT16_MAX,true},
        {3,0,false},{3,1,false},{3,2,false},{3,3,true},{3,UINT16_MAX,true}};
    for (const auto& c : cases) {
        sdk::reset(); Fixture f; auto requested = timing;
        switch (c.field) {
            case 0: requested.hActive = c.width; break;
            case 1: requested.hFrontPorch = c.width; break;
            case 2: requested.hSyncWidth = c.width; break;
            default: requested.hBackPorch = c.width; break;
        }
        HardwareScanlineEngine engine(f.config()); CHECK_EQ(engine.configure(requested), c.valid);
        if (!c.valid) { checkRejectedStart(engine); checkNoPinChanges(); checkReleased(); continue; }
        const auto segments = HardwareScanlineEngine::segmentsPerLine(requested);
        for (unsigned line = 0; line < requested.vTotal(); ++line) {
            unsigned cycles = 0;
            for (unsigned segment = 0; segment < segments; ++segment) cycles += (f.table[line * segments + segment] >> 3) + 3;
            CHECK_EQ(cycles, requested.hTotal());
        }
        CHECK_EQ(sdk::recording.dma[0].count, requested.vTotal() * segments);
        CHECK(engine.start()); engine.stop();
    }
}

PICOTTL_TEST(engineMaximumTimingComponentsFitSumsTableAndDma) {
    constexpr VideoTiming maximum{10'000'000,UINT16_MAX,UINT16_MAX,UINT16_MAX,UINT16_MAX,
        UINT16_MAX,UINT16_MAX,UINT16_MAX,UINT16_MAX,picottl::SyncPolarity::ActiveHigh,picottl::SyncPolarity::ActiveLow};
    static_assert(maximum.hTotal() == 262140 && maximum.vTotal() == 262140, "Totals exceed 16 bits, not 32 bits");
    constexpr auto words = HardwareScanlineEngine::timingWordsFor(maximum);
    static_assert(words == 1048560, "Maximum table fits the normal DMA count field");
    sdk::reset(); Fixture f;
    std::vector<std::uint32_t> table(words + 2u, 0xdeadbeefu);
    auto config = f.config(); config.timingBuffer = table.data() + 1; config.timingBufferWords = words;
    {
        HardwareScanlineEngine engine(config); CHECK(engine.configure(maximum));
        CHECK_EQ(sdk::recording.dma[0].count, words);
        CHECK(sdk::recording.dma[0].count <= DMA_CH0_TRANS_COUNT_COUNT_BITS);
        CHECK_EQ(table.front(), 0xdeadbeefu); CHECK_EQ(table.back(), 0xdeadbeefu);
        bool delaysMatch = true;
        for (std::size_t i = 1; i <= words; ++i) delaysMatch = delaysMatch && (table[i] >> 3) == UINT16_MAX - 3u;
        CHECK(delaysMatch);
        CHECK(engine.start()); engine.stop();
    }
    checkReleased();
}

PICOTTL_TEST(engineRejectedPolarityReconfigurationRetainsOnlyOwnedResources) {
    sdk::reset(); Fixture f; sdk::recording.dmaClaimed[9] = true;
    sdk::recording.smClaimed[0][3] = true; sdk::recording.smEnabled[0][3] = true;
    {
        HardwareScanlineEngine engine(f.config()); CHECK(engine.setPixelSource(f.pixels, 4));
        CHECK(engine.configure(timing)); CHECK(engine.start());
        const auto mux = sdk::recording.gpioConfigurations;
        const auto claims = sdk::recording.dmaClaimed;
        const auto stateMachines = sdk::recording.smClaimed;
        auto invalid = timing; invalid.vSyncPolarity = static_cast<picottl::SyncPolarity>(2);
        CHECK(!engine.configure(invalid)); checkRejectedStart(engine); checkQuiescent(15);
        CHECK_EQ(sdk::recording.gpioConfigurations, mux);
        CHECK(sdk::recording.dmaClaimed == claims); CHECK(sdk::recording.smClaimed == stateMachines);
        CHECK(sdk::recording.smEnabled[0][3]); CHECK(sdk::recording.dmaClaimed[9]);
        CHECK(engine.configure(timing)); CHECK(engine.start());
    }
    checkReleased(); CHECK(sdk::recording.smClaimed[0][3]); CHECK(sdk::recording.dmaClaimed[9]);
}

PICOTTL_TEST(engineDmaProgrammingSurvivesRestartAndSourceReplacement) {
    // Nondefault channels and state machines expose accidental fixed mappings.
    for (unsigned pio = 0; pio < 3; ++pio) {
        for (const std::uint8_t depth : {1, 2, 4, 8}) {
            sdk::reset(); Fixture f;
            auto c = f.config();
            c.pioInstance = static_cast<std::uint8_t>(pio);
            c.stateMachine = 2; c.pixelStateMachine = 3;
            c.dmaDataChannel = 4; c.dmaControlChannel = 5;
            c.pixelDmaDataChannel = 6; c.pixelDmaControlChannel = 7;
            alignas(std::uint32_t) std::uint8_t replacement[32]{};
            {
                HardwareScanlineEngine engine(c);
                const auto verify = [&](const std::uint8_t* source) {
                    // Exact SDK CTRL_TRIG encoding: word transfers, incrementing
                    // reads only, correct TX DREQ, and byte swap only for pixels.
                    const std::uint32_t wordEnabled = 1u | (2u << 2u);
                    const auto& timingDma = sdk::recording.dma[4];
                    const auto& pixelDma = sdk::recording.dma[6];
                    CHECK_EQ(timingDma.configuredCtrl, wordEnabled | 0x10u |
                        ((pio * 8u + 2u) << 17u) | (5u << 13u));
                    CHECK_EQ(pixelDma.configuredCtrl, wordEnabled | 0x10u |
                        ((pio * 8u + 3u) << 17u) | (7u << 13u) | 0x01000000u);
                    CHECK(timingDma.read == f.table);
                    CHECK(timingDma.write == &sdk::pios[pio].txf[2]);
                    CHECK_EQ(timingDma.count, 28u);
                    CHECK(pixelDma.read == source);
                    CHECK(pixelDma.write == &sdk::pios[pio].txf[3]);
                    CHECK_EQ(pixelDma.count, static_cast<std::uint32_t>(depth));
                    for (const unsigned control : {5u, 7u}) {
                        const auto& rewind = sdk::recording.dma[control];
                        CHECK_EQ(rewind.configuredCtrl, wordEnabled |
                            (63u << 17u) | (control << 13u));
                        CHECK_EQ(rewind.count, 1u);
                        CHECK(rewind.write == &dma_hw->ch[control - 1u].al3_read_addr_trig);
                        CHECK(rewind.read != nullptr);
                        const auto expected = control == 5u
                            ? static_cast<const void*>(f.table)
                            : static_cast<const void*>(source);
                        CHECK_EQ(*static_cast<const volatile std::uint32_t*>(rewind.read),
                            static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(expected)));
                    }
                };
                CHECK(engine.setPixelSource(f.pixels, std::size_t{4} * depth, depth));
                CHECK(engine.configure(timing)); verify(f.pixels);
                for (unsigned cycle = 0; cycle < 2; ++cycle) {
                    CHECK(engine.start()); verify(f.pixels); engine.stop();
                }
                CHECK(engine.setPixelSource(replacement, std::size_t{4} * depth, depth));
                CHECK(engine.configure(timing));
                CHECK(engine.start()); verify(replacement); engine.stop();
            }
            for (unsigned channel = 4; channel <= 7; ++channel)
                CHECK(!sdk::recording.dmaClaimed[channel]);
            CHECK(!sdk::recording.smClaimed[pio][2]);
            CHECK(!sdk::recording.smClaimed[pio][3]);
        }
    }
}

PICOTTL_TEST(engineTimingTableExactAndOneWordShortCapacities) {
    for (const std::uint16_t front : {0, 4}) {
        auto requested = timing; requested.hFrontPorch = front;
        const auto required = HardwareScanlineEngine::timingWordsFor(requested);
        for (const auto capacity : {required - 1, required}) {
            sdk::reset(); Fixture f;
            std::vector<std::uint32_t> table(required + 2, 0xfeedcafeu);
            auto config = f.config(); config.timingBuffer = table.data() + 1;
            config.timingBufferWords = capacity;
            {
                HardwareScanlineEngine engine(config);
                if (capacity < required) {
                    CHECK(!engine.configure(requested)); checkRejectedStart(engine);
                    CHECK_EQ(sdk::recording.calls, 0u); checkNoPinChanges(); checkReleased();
                    for (const auto word : table) CHECK_EQ(word, 0xfeedcafeu);
                    --requested.vBackPorch; // valid retry fits the same declared capacity
                    CHECK(engine.configure(requested));
                } else {
                    CHECK(engine.configure(requested));
                }
                CHECK(engine.start()); engine.stop(); CHECK(engine.start()); engine.stop();
                CHECK(sdk::recording.dma[0].read == table.data() + 1);
                CHECK_EQ(sdk::recording.dma[0].count,
                    HardwareScanlineEngine::timingWordsFor(requested));
                CHECK(sdk::recording.dma[0].count <= capacity);
            }
            CHECK_EQ(table.front(), 0xfeedcafeu); CHECK_EQ(table.back(), 0xfeedcafeu);
            checkReleased();
            requested = timing; requested.hFrontPorch = front;
        }
    }
}

PICOTTL_TEST(engineRejectedPixelSourcesPreserveConfiguredBinding) {
    for (unsigned failure = 0; failure < 6; ++failure) {
        sdk::reset(); Fixture f;
        alignas(std::uint32_t) std::uint8_t replacement[4]{};
        HardwareScanlineEngine engine(f.config());
        CHECK(engine.setPixelSource(f.pixels, 4)); CHECK(engine.configure(timing));
        const auto calls = sdk::recording.calls;
        const auto* candidate = failure == 0 ? f.pixels + 1 : f.pixels;
        const std::size_t bytes = failure == 1 ? 0u : failure == 2 ? 3u : failure == 3 ? 5u : 4u;
        const std::uint8_t depth = failure == 4 ? 6 : 1;
        const std::uint8_t pins = failure == 5 ? 2 : 1;
        CHECK(!engine.setPixelSource(candidate, bytes, depth, pins));
        CHECK_EQ(sdk::recording.calls, calls);
        CHECK(engine.start()); CHECK(sdk::recording.dma[2].read == f.pixels);
        CHECK_EQ(sdk::recording.dma[2].count, 1u); engine.stop();
        CHECK(engine.setPixelSource(replacement, sizeof replacement));
        CHECK(engine.configure(timing)); CHECK(engine.start());
        CHECK(sdk::recording.dma[2].read == replacement); engine.stop();
        CHECK(engine.setPixelSource(nullptr, 0)); checkRejectedStart(engine);
    }
}
