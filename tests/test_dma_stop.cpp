// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#include "../src/rp2350/DmaStop.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace {

// Records the production ordering helper's contract only. This is not a
// model of RP2350 bus timing, DREQ credits, PIO or the electrical erratum.
struct DmaRecorder {
    struct Channel {
        bool enabled = false;
        bool busy = false;
        unsigned chainTo = 0;
    };
    std::array<Channel, 16> channels{};
    std::vector<unsigned> disabled;
    std::uint32_t expectedMask = 0;
    std::uint32_t abortedMask = 0;
    unsigned aborts = 0;
    unsigned polls = 0;
    unsigned waits = 0;
    bool pendingControlWrite = false;
    unsigned checkedDisabledChannels = 0;

    void configurePair(unsigned data, unsigned control, bool active) {
        expectedMask |= (1u << data) | (1u << control);
        channels[data] = {true, active, control};
        channels[control] = {true, active, control};
    }

    void stop() {
        picottl::rp2350::detail::stopDmaChannels(expectedMask,
            [this](std::uint8_t channel) {
                CHECK_EQ(aborts, 0u);
                channels[channel].enabled = false;
                channels[channel].chainTo = channel;
                disabled.push_back(channel);
            },
            [this](std::uint32_t mask) {
                CHECK_EQ(mask, expectedMask);
                for (unsigned channel = 0; channel < channels.size(); ++channel) {
                    if ((mask & (1u << channel)) != 0) {
                        CHECK(!channels[channel].enabled);
                        CHECK_EQ(channels[channel].chainTo, channel);
                    }
                }
                abortedMask = mask;
                ++aborts;
            },
            [this](std::uint32_t mask) {
                CHECK_EQ(mask, abortedMask);
                ++polls;
                // Supply pending then complete: the helper must wait for
                // retirement, even when a control write was already issued.
                if (polls == 1) {
                    if (pendingControlWrite) {
                        for (unsigned channel = 0; channel < channels.size(); ++channel) {
                            if ((mask & (1u << channel)) != 0) {
                                CHECK(!channels[channel].enabled);
                                ++checkedDisabledChannels;
                            }
                        }
                    }
                    return true;
                }
                for (unsigned channel = 0; channel < channels.size(); ++channel) {
                    if ((mask & (1u << channel)) != 0) {
                        channels[channel].busy = false;
                    }
                }
                return false;
            },
            [this] { ++waits; });
    }

    void checkStopped() const {
        CHECK_EQ(aborts, 1u);
        CHECK_EQ(abortedMask, expectedMask);
        CHECK_EQ(polls, 2u);
        CHECK_EQ(waits, 1u);
        for (unsigned channel = 0; channel < channels.size(); ++channel) {
            if ((expectedMask & (1u << channel)) != 0) {
                CHECK(!channels[channel].enabled);
                CHECK(!channels[channel].busy);
                CHECK_EQ(channels[channel].chainTo, channel);
            }
        }
    }

    void resetRecording() {
        disabled.clear();
        abortedMask = 0;
        aborts = polls = waits = 0;
    }
};

} // namespace

PICOTTL_TEST(dmaStopActiveTimingPairWithoutFramebuffer) {
    DmaRecorder dma;
    dma.configurePair(0, 1, true);
    dma.stop();
    dma.checkStopped();
    CHECK_EQ(dma.disabled.size(), 2u);
}

PICOTTL_TEST(dmaStopBothChainedPairsWithFramebuffer) {
    DmaRecorder dma;
    dma.configurePair(0, 1, true);
    dma.configurePair(2, 3, true);
    dma.pendingControlWrite = true;
    dma.stop();
    dma.checkStopped();
    CHECK_EQ(dma.disabled.size(), 4u);
    CHECK_EQ(dma.checkedDisabledChannels, 4u);
}

PICOTTL_TEST(dmaStopRepeatedAndReleasePreconditions) {
    DmaRecorder dma;
    dma.configurePair(0, 1, true);
    dma.configurePair(2, 3, true);
    dma.stop();
    dma.checkStopped();
    dma.resetRecording();
    dma.stop();
    dma.checkStopped(); // all owned channels remain disabled and retired
}

PICOTTL_TEST(dmaStopReconfiguredPairs) {
    DmaRecorder dma;
    dma.configurePair(0, 1, true);
    dma.configurePair(2, 3, true);
    dma.stop();
    dma.checkStopped();
    dma.resetRecording();
    // start() now calls setupDma/setupPixelDma before triggering. Here
    // supply that precondition; this fixture does not execute start().
    dma.configurePair(0, 1, true);
    dma.configurePair(2, 3, true);
    dma.stop();
    dma.checkStopped();
}

PICOTTL_TEST(dmaStopOwnedPixelPairAfterFramebufferDetach) {
    DmaRecorder dma;
    dma.configurePair(0, 1, true);
    // Pixel resources remain owned after detach: ownership, not the
    // current source pointer, must determine the abort mask.
    dma.configurePair(2, 3, false);
    dma.stop();
    dma.checkStopped();
    CHECK_EQ(dma.abortedMask, 15u);
}

PICOTTL_TEST(dmaStopPartiallyConfiguredIdleTimingPair) {
    DmaRecorder dma;
    dma.configurePair(0, 1, false);
    dma.stop();
    dma.checkStopped();
}

PICOTTL_TEST(dmaStopDoesNotTouchUnownedChannels) {
    DmaRecorder dma;
    dma.configurePair(4, 7, true);
    dma.channels[2] = {true, true, 3};
    dma.stop();
    dma.checkStopped();
    CHECK(dma.channels[2].enabled);
    CHECK(dma.channels[2].busy);
    CHECK_EQ(dma.channels[2].chainTo, 3u);
    CHECK_EQ(dma.disabled[0], 4u);
    CHECK_EQ(dma.disabled[1], 7u);
}

PICOTTL_TEST(dmaStopWithoutAcquiredResourcesDoesNothing) {
    DmaRecorder dma;
    dma.stop();
    CHECK(dma.disabled.empty());
    CHECK_EQ(dma.aborts, 0u);
    CHECK_EQ(dma.polls, 0u);
    CHECK_EQ(dma.waits, 0u);
}

PICOTTL_TEST(dmaStopAlreadyRetiredNeedsNoWait) {
    unsigned aborts = 0;
    unsigned waits = 0;
    picottl::rp2350::detail::stopDmaChannels(3,
        [](std::uint8_t) {},
        [&aborts](std::uint32_t) { ++aborts; },
        [](std::uint32_t) { return false; },
        [&waits] { ++waits; });
    CHECK_EQ(aborts, 1u);
    CHECK_EQ(waits, 0u);
}
