// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#include "picottl/PackedFramebuffer.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"
#include "support/pico_sdk/RecordingPicoSdk.hpp"
#include <limits>

#if PICOTTL_TEST_GPIO_COUNT == 48
#define MDA_TEST(name) PICOTTL_TEST(mdaB##name)
#else
#define MDA_TEST(name) PICOTTL_TEST(mdaA##name)
#endif

namespace {
namespace sdk = picottl_test::sdk;
using Device = picottl::rp2350::MdaDisplayDevice;
constexpr auto mode = picottl::VideoMode::IbmMda;
alignas(4) std::uint8_t storage[63000]{};
Device::Config config(picottl::Framebuffer* frame) {
    Device::Config c;
    c.hsyncPin = 2; c.videoPin = 16; c.framebuffer = frame;
    return c;
}
void noOwnedResources() {
    for (bool claimed : sdk::recording.dmaClaimed) CHECK(!claimed);
    for (const auto& pio : sdk::recording.smClaimed) for (bool claimed : pio) CHECK(!claimed);
    for (const auto& pio : sdk::recording.programs) for (bool loaded : pio) CHECK(!loaded);
}
void noHardwareChanges() {
    noOwnedResources();
    CHECK_EQ(sdk::recording.gpioConfigurations, 0u);
    CHECK_EQ(sdk::recording.pinDirections, 0u);
    CHECK_EQ(sdk::recording.padConfigurations, 0u);
    CHECK_EQ(sdk::recording.pinWrites, 0u);
    CHECK_EQ(sdk::recording.triggers, 0u);
    for (const auto& pio : sdk::recording.smEnabled) for (bool enabled : pio) CHECK(!enabled);
    for (const auto& dma : sdk::recording.dma) CHECK_EQ(dma.configurations, 0u);
}
void checkRunning(Device& device, unsigned video, unsigned depth) {
    CHECK(device.isActive());
    CHECK(sdk::recording.smEnabled[0][0]); CHECK(sdk::recording.smEnabled[0][1]);
    CHECK_EQ(sdk::recording.outRelative[0][1], video - sdk::recording.gpioBase[0]);
    for (unsigned bit = 0; bit < depth; ++bit) CHECK_EQ(sdk::recording.gpioFunction[video + bit], 1);
    CHECK_EQ(sdk::recording.dma[2].count, 31500u * depth / 4u);
    CHECK_EQ(sdk::recording.invalidPinCalls, 0u);
}
}

MDA_TEST(RejectsNarrowedExplicitMonoPins) {
    picottl::MonochromeFramebuffer frame(storage, sizeof storage, 720, 350);
    const std::int16_t pins[] = {272, 273, 256, 257, 32752, std::numeric_limits<std::int16_t>::max()};
    for (auto pin : pins) {
        sdk::reset(); auto c = config(&frame); c.pixelPins[0] = pin;
        {
            Device device(c);
            for (unsigned repeat = 0; repeat < 3; ++repeat) {
                CHECK(!device.begin(mode)); CHECK(!device.isActive()); noHardwareChanges();
            }
        }
        noOwnedResources();
    }
}

MDA_TEST(RejectsNarrowedPackedPairAndLeavesResourcesAvailable) {
    picottl::PackedFramebuffer<2> frame(storage, sizeof storage, 720, 350);
    sdk::reset(); auto c = config(&frame); c.pixelPins[0] = 272; c.pixelPins[1] = 273;
    {
        Device invalid(c); CHECK(!invalid.begin(mode)); CHECK(!invalid.isActive()); noHardwareChanges();
        {
            Device valid(config(&frame)); CHECK(valid.begin(mode)); checkRunning(valid, 16, 2);
            valid.end(); CHECK(valid.begin(mode)); checkRunning(valid, 16, 2);
        }
        noOwnedResources();
    }
    noOwnedResources();
}

MDA_TEST(PreservesPackageLimitsAndHighGpioWindow) {
    picottl::MonochromeFramebuffer frame(storage, sizeof storage, 720, 350);
    for (unsigned pin : {NUM_BANK0_GPIOS - 1u, static_cast<unsigned>(NUM_BANK0_GPIOS)}) {
        sdk::reset(); auto c = config(&frame);
        if (NUM_BANK0_GPIOS == 48) { sdk::recording.gpioBase[0] = 16; c.hsyncPin = 34; }
        c.pixelPins[0] = static_cast<std::int16_t>(pin);
        {
            Device device(c);
            CHECK_EQ(device.begin(mode), pin < NUM_BANK0_GPIOS);
            if (pin < NUM_BANK0_GPIOS) { checkRunning(device, pin, 1); device.end(); }
            else { CHECK(!device.isActive()); noHardwareChanges(); }
        }
        noOwnedResources();
    }
    sdk::reset(); auto c = config(&frame); c.pixelPins[0] = 32;
    { Device device(c); CHECK(!device.begin(mode)); noHardwareChanges(); }
    picottl::PackedFramebuffer<2> packed(storage, sizeof storage, 720, 350);
    for (bool explicitPins : {false, true}) {
        sdk::reset(); auto high = config(&packed);
        if (NUM_BANK0_GPIOS == 48) { sdk::recording.gpioBase[0] = 16; high.hsyncPin = 34; }
        high.videoPin = NUM_BANK0_GPIOS - 2;
        if (explicitPins) { high.pixelPins[0] = NUM_BANK0_GPIOS - 2; high.pixelPins[1] = NUM_BANK0_GPIOS - 1; }
        {
            Device device(high); CHECK(device.begin(mode)); checkRunning(device, NUM_BANK0_GPIOS - 2, 2);
            device.end(); CHECK(device.begin(mode)); checkRunning(device, NUM_BANK0_GPIOS - 2, 2);
        }
        noOwnedResources();
    }
}

MDA_TEST(PreservesFallbackAndMixedExplicitPins) {
    picottl::PackedFramebuffer<2> frame(storage, sizeof storage, 720, 350);
    for (unsigned variant = 0; variant < 4; ++variant) {
        sdk::reset(); auto c = config(&frame);
        if (variant == 1) { c.pixelPins[0] = 16; }
        if (variant == 2) { c.pixelPins[1] = 17; }
        if (variant == 3) { c.pixelPins[0] = std::numeric_limits<std::int16_t>::min(); c.pixelPins[1] = -2; }
        { Device device(c); CHECK(device.begin(mode)); checkRunning(device, 16, 2); device.end(); }
        noOwnedResources();
    }
    sdk::reset();
    { Device device(Device::Config{}); CHECK(device.begin(mode)); CHECK(device.isActive()); device.end(); }
    noOwnedResources();
}

MDA_TEST(RetriesWithOnlyTheActivePixelPins) {
    picottl::PackedFramebuffer<2> packed(storage, sizeof storage, 720, 350);
    picottl::MonochromeFramebuffer mono(storage, sizeof storage, 720, 350);
    sdk::reset(); auto c = config(&packed); c.pixelPins[0] = 16; c.pixelPins[1] = 273;
    {
        Device device(c); CHECK(!device.begin(mode)); CHECK(!device.isActive()); noHardwareChanges();
        CHECK(device.setFramebuffer(&mono)); CHECK(device.begin(mode)); checkRunning(device, 16, 1);
        device.end(); CHECK(device.setFramebuffer(&packed));
        const auto gpioCalls = sdk::recording.gpioConfigurations;
        CHECK(!device.begin(mode)); CHECK(!device.isActive());
        CHECK_EQ(sdk::recording.gpioConfigurations, gpioCalls);
        CHECK(!sdk::recording.smEnabled[0][0]); CHECK(!sdk::recording.smEnabled[0][1]);
        CHECK(device.setFramebuffer(&mono)); CHECK(device.begin(mode)); checkRunning(device, 16, 1);
    }
    noOwnedResources();
}

MDA_TEST(IgnoresUnusedPinsForSignalOnlyRecovery) {
    picottl::MonochromeFramebuffer mono(storage, sizeof storage, 720, 350);
    sdk::reset(); auto c = config(&mono); c.pixelPins[0] = 272;
    {
        Device device(c); CHECK(!device.begin(mode)); noHardwareChanges();
        CHECK(device.setFramebuffer(nullptr)); CHECK(device.begin(mode)); CHECK(device.isActive());
        CHECK(!sdk::recording.smClaimed[0][1]); CHECK(!sdk::recording.dmaClaimed[2]);
        device.end(); CHECK(device.setFramebuffer(&mono));
        const auto gpioCalls = sdk::recording.gpioConfigurations;
        CHECK(!device.begin(mode)); CHECK(!device.isActive());
        CHECK_EQ(sdk::recording.gpioConfigurations, gpioCalls);
    }
    noOwnedResources();
}
