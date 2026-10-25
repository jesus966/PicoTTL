// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#include "picottl/video/ModeTimings.hpp"

namespace {
using picottl::VideoTiming;

constexpr VideoTiming custom(std::uint32_t clock, std::uint16_t horizontal,
                             std::uint16_t vertical, std::uint16_t hFront = 0,
                             std::uint16_t vFront = 0) {
    return {clock, horizontal, hFront, 0, 0, vertical, vFront, 0, 0,
            picottl::SyncPolarity::ActiveHigh, picottl::SyncPolarity::ActiveLow};
}

struct RateCase {
    VideoTiming timing;
    std::uint32_t horizontal, vertical, lineHz, refreshMilliHz;
};

constexpr RateCase cases[] = {
    {picottl::modes::kIbmMda, 882, 370, 18'431, 49'816},
    {picottl::modes::kMdaOverscan, 882, 370, 18'431, 49'816},
    {picottl::modes::kIbmCga640, 912, 262, 15'699, 59'922},
    {picottl::modes::kIbmCga320, 456, 262, 15'699, 59'922},
    {picottl::modes::kIbmEga350, 744, 364, 21'850, 60'029},
    {custom(101, 3, 2), 3, 2, 33, 16'833},
    {custom(100'000'000, 65'535, 65'535), 65'535, 65'535, 1525, 23},
    {custom(100'000'000, 65'535, 65'535, 0, 2), 65'535, 65'537, 1525, 23},
    // Frame sizes at and above 2^32, including a nonzero wrapped denominator.
    {custom(100'000'000, 65'535, 65'535, 1, 1), 65'536, 65'536, 1525, 23},
    {custom(100'000'000, 65'535, 65'535, 2, 1), 65'537, 65'536, 1525, 23},
    // Maximum components sum to 262140, without overflowing either total.
    {{UINT32_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
      UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX,
      picottl::SyncPolarity::ActiveHigh, picottl::SyncPolarity::ActiveLow},
      262'140, 262'140, 16'384, 62},
    {custom(100, 0, 10), 0, 10, 0, 0},
    {custom(100, 10, 0), 10, 0, 10, 0},
    {custom(0, 0, 0), 0, 0, 0, 0},
    {custom(0, 10, 10), 10, 10, 0, 0},
    {custom(1, 65'535, 65'535, 1, 1), 65'536, 65'536, 0, 0},
    // Exact maximum return value, and the first integer above it.
    {custom(UINT32_MAX, 1, 1000), 1, 1000, UINT32_MAX, UINT32_MAX},
    {custom(536'870'912, 5, 25), 5, 25, 107'374'182, 0},
    {custom(UINT32_MAX, 1, 1), 1, 1, UINT32_MAX, 0},
    // The unrounded quotient exceeds UINT32_MAX, but its floor still fits.
    {custom(188'978'561, 4, 11), 4, 11, 47'244'640, UINT32_MAX},
};

constexpr bool ratesMatch() {
    for (const auto& item : cases) {
        if (item.timing.hTotal() != item.horizontal ||
            item.timing.vTotal() != item.vertical ||
            item.timing.lineFrequencyHz() != item.lineHz ||
            item.timing.refreshRateMilliHz() != item.refreshMilliHz) {
            return false;
        }
    }
    return true;
}
static_assert(ratesMatch(), "Timing helpers must remain correct in constant expressions");
} // namespace

PICOTTL_TEST(videoTimingRatesMatchExactResultsAtRuntime) {
    // Run all cases repeatedly as well as evaluating them at compile time.
    for (unsigned repeat = 0; repeat < 3; ++repeat) {
        for (const auto& item : cases) {
            CHECK_EQ(item.timing.hTotal(), item.horizontal);
            CHECK_EQ(item.timing.vTotal(), item.vertical);
            CHECK_EQ(item.timing.lineFrequencyHz(), item.lineHz);
            CHECK_EQ(item.timing.refreshRateMilliHz(), item.refreshMilliHz);
        }
    }
}
