// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#include "support/HardwareScanlineEngine.hpp"
#include "support/pico_sdk/RecordingPicoSdk.hpp"

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

PICOTTL_TEST(engineLatePixelClockFailureRecoversAndRearmsBothPairs) {
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
        CHECK_EQ(sdk::recording.calls, 0u);
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

PICOTTL_TEST(enginePixelParameterFailuresCleanUpTheirActualPartialAcquisition) {
    for (unsigned failure = 0; failure < 6; ++failure) {
        sdk::reset(); Fixture f;
        auto config = f.config();
        switch (failure) {
            case 0: config.pixelStateMachine = config.stateMachine; break;
            case 1: config.pixelDmaDataChannel = NUM_DMA_CHANNELS; break;
            case 2: config.pixelDmaControlChannel = config.pixelDmaDataChannel; break;
            case 3: config.pixelDmaDataChannel = config.dmaControlChannel; break;
            case 4: config.hsyncPin = NUM_BANK0_GPIOS - 2; break;
            default: config.videoPin = config.hsyncPin; break;
        }
        {
            HardwareScanlineEngine engine(config);
            CHECK(engine.setPixelSource(f.pixels, 4));
            CHECK(!engine.configure(timing));
            checkRejectedStart(engine);
            CHECK(sdk::recording.dmaClaimed[0]);
            CHECK(sdk::recording.dmaClaimed[1]);
            engine.stop(); engine.stop();
            checkQuiescent(failure == 5 ? 15 : 3);
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
