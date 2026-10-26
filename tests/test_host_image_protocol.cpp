// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#include "../examples/43_host_image/ImageReceiver.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>

namespace {
using host_image::ReceiveError;
constexpr std::uint32_t capacity = 224'000;
constexpr std::uint32_t sizes[] = {63'000, 128'000, 64'000, capacity};
struct Commands {
    unsigned hellos = 0, modeCalls = 0, modeErrors = 0, acks = 0, errors = 0;
    unsigned mode = 0;
    ReceiveError lastError = ReceiveError::Length;
    std::uint32_t expected = 0;
    void hello() { ++hellos; }
    bool setMode(std::uint8_t index) {
        ++modeCalls;
        if (index >= 4) return false;
        mode = index;
        return true;
    }
    std::uint32_t frameBytes() const { return sizes[mode]; }
    void modeResult(bool accepted) { if (!accepted) ++modeErrors; }
    void frameAccepted() { ++acks; }
    void error(ReceiveError value, std::uint32_t count) { ++errors; lastError = value; expected = count; }
};
struct Fixture {
    std::array<std::uint8_t, capacity + 2> bytes{};
    Commands commands;
    host_image::ImageReceiver<Commands> receiver{bytes.data() + 1, capacity, commands};
    std::uint32_t now = 0;
    Fixture() { bytes.fill(0x5A); bytes.front() = bytes.back() = 0xA5; }
    void byte(std::uint8_t value) { receiver.poll(value, now, true); }
    void advance(std::uint32_t delta, bool connected = true) { now += delta; receiver.poll(-1, now, connected); }
    void frame(std::uint32_t length) {
        byte('F');
        for (unsigned i = 0; i < 4; ++i) byte(static_cast<std::uint8_t>(length >> (8u * i)));
    }
    void payload(std::uint32_t count, std::uint8_t value = 0x35) {
        for (std::uint32_t i = 0; i < count; ++i) byte(value);
    }
    void guards() { CHECK_EQ(bytes.front(), 0xA5); CHECK_EQ(bytes.back(), 0xA5); }
};
constexpr auto timeout = host_image::ImageReceiver<Commands>::kInactivityMs;
} // namespace

PICOTTL_TEST(hostImageValidModesAndConsecutiveFrames) {
    for (std::uint8_t mode = 0; mode < 4; ++mode) {
        Fixture f;
        f.byte('H'); f.byte('M'); f.byte(mode);
        CHECK_EQ(f.commands.hellos, 1u);
        CHECK_EQ(f.commands.mode, mode);
        for (std::uint8_t value : {0x35, 0x19}) {
            f.frame(sizes[mode]); f.payload(sizes[mode], value);
            CHECK_EQ(f.bytes[1], value);
            CHECK_EQ(f.bytes[sizes[mode]], value);
        }
        CHECK_EQ(f.commands.acks, 2u);
        CHECK_EQ(f.commands.errors, 0u);
        f.guards();
    }
}

PICOTTL_TEST(hostImageExactCapacityTreatsCommandBytesAsPixels) {
    Fixture f;
    f.byte('M'); f.byte(3); f.frame(capacity);
    const std::uint8_t pattern[] = {'H', 'M', 'F'};
    for (std::uint32_t i = 0; i < capacity; ++i) f.byte(pattern[i % 3]);
    CHECK_EQ(f.commands.hellos, 0u);
    CHECK_EQ(f.commands.modeCalls, 1u);
    CHECK_EQ(f.commands.acks, 1u);
    for (std::uint32_t i = 0; i < capacity; ++i) CHECK_EQ(f.bytes[i + 1], pattern[i % 3]);
    f.guards();
}

PICOTTL_TEST(hostImageZeroLengthRetainsKnownCommandBoundary) {
    Fixture f;
    f.frame(0);
    CHECK(!f.receiver.requiresReset());
    CHECK_EQ(f.commands.errors, 1u);
    CHECK_EQ(f.commands.expected, sizes[0]);
    CHECK_EQ(f.commands.acks, 0u);
    f.byte('H'); f.byte('M'); f.byte(255);
    CHECK_EQ(f.commands.hellos, 1u);
    CHECK_EQ(f.commands.modeErrors, 1u);
    f.frame(sizes[0]); f.payload(sizes[0]);
    CHECK_EQ(f.commands.acks, 1u);
    f.guards();
}

PICOTTL_TEST(hostImageInvalidLengthsQuarantineWithoutDraining) {
    for (std::uint32_t length : {sizes[0] + 1, capacity + 1, capacity + 8, UINT32_MAX}) {
        Fixture f;
        f.frame(length); // no body supplied: rejection must already be complete
        CHECK(f.receiver.requiresReset());
        CHECK_EQ(f.commands.errors, 1u);
        CHECK(f.commands.lastError == ReceiveError::Length);
        CHECK_EQ(f.commands.expected, sizes[0]);
        // Complete feasible rejected bodies. In the capacity+8 case these
        // command-like suffix bytes were previously left for dispatch.
        if (length != UINT32_MAX) f.payload(length - 8, 0);
        for (std::uint8_t byte : std::array<std::uint8_t, 8>{'H', 'M', 0, 'F', 0, 0, 0, 0}) f.byte(byte);
        CHECK_EQ(f.commands.hellos, 0u);
        CHECK_EQ(f.commands.modeCalls, 0u);
        CHECK_EQ(f.commands.acks, 0u);
        CHECK_EQ(f.bytes[1], 0x5A);
        f.guards();
    }
}

PICOTTL_TEST(hostImagePartialHeadersTimeOut) {
    for (unsigned count = 0; count < 4; ++count) {
        Fixture f;
        f.byte('F');
        for (unsigned i = 0; i < count; ++i) f.byte(0);
        f.advance(timeout - 1);
        CHECK(!f.receiver.requiresReset());
        f.advance(1);
        CHECK(f.receiver.requiresReset());
        CHECK(f.commands.lastError == ReceiveError::Timeout);
        CHECK_EQ(f.commands.acks, 0u);
        CHECK_EQ(f.bytes[1], 0x5A);
        f.guards();
    }
    Fixture f;
    f.byte('M'); f.advance(timeout);
    CHECK(f.receiver.requiresReset());
    CHECK_EQ(f.commands.modeCalls, 0u);
}

PICOTTL_TEST(hostImagePartialPayloadTimeoutClearsIncompleteImage) {
    Fixture f;
    f.frame(sizes[0]); f.payload(2);
    CHECK_EQ(f.bytes[1], 0x35);
    f.advance(timeout);
    CHECK(f.receiver.requiresReset());
    CHECK(f.commands.lastError == ReceiveError::Timeout);
    CHECK_EQ(f.commands.acks, 0u);
    for (std::uint32_t i = 1; i <= sizes[0]; ++i) CHECK_EQ(f.bytes[i], 0);
    CHECK_EQ(f.bytes[sizes[0] + 1], 0x5A);
    f.guards();
}

PICOTTL_TEST(hostImageDisconnectDuringReceptionRequiresReset) {
    for (unsigned phase = 0; phase < 3; ++phase) {
        Fixture f;
        if (phase == 0) { f.byte('F'); f.byte(0); }
        else if (phase == 1) { f.frame(sizes[0]); f.payload(2); }
        else f.byte('M');
        f.advance(1, false);
        CHECK(f.receiver.requiresReset());
        CHECK(f.commands.lastError == ReceiveError::Disconnected);
        f.byte('H');
        CHECK_EQ(f.commands.hellos, 0u);
        CHECK_EQ(f.commands.acks, 0u);
        CHECK_EQ(f.bytes[1], phase == 1 ? 0 : 0x5A);
        f.guards();
    }
}

PICOTTL_TEST(hostImageProgressRenewsInactivityDeadline) {
    Fixture f;
    f.frame(sizes[0]);
    for (unsigned i = 0; i < 3; ++i) {
        f.advance(timeout - 1); f.byte(0x35);
        CHECK(!f.receiver.requiresReset());
    }
    CHECK(f.now > timeout); // no arbitrary total-duration deadline
    f.payload(sizes[0] - 3);
    CHECK_EQ(f.commands.acks, 1u);
    CHECK_EQ(f.commands.errors, 0u);
}

PICOTTL_TEST(hostImageInactivityTimeoutHandlesTimerWrap) {
    Fixture f;
    f.now = UINT32_MAX - timeout / 2;
    f.frame(sizes[0]); f.byte(1);
    f.advance(timeout - 1);
    CHECK(!f.receiver.requiresReset());
    f.advance(1);
    CHECK(f.receiver.requiresReset());
    CHECK(f.commands.lastError == ReceiveError::Timeout);
}

PICOTTL_TEST(hostImageAmbiguousSessionNeedsFirmwareResetNotReconnect) {
    for (bool timedOut : {false, true}) {
        Fixture f;
        if (timedOut) { f.frame(sizes[0]); f.byte(1); f.advance(timeout); }
        else f.frame(UINT32_MAX);
        f.advance(1, false); // reconnecting does not reset a quarantined stream
        f.byte('H'); f.byte('M'); f.byte(0);
        f.frame(sizes[0]); f.payload(sizes[0]);
        CHECK(f.receiver.requiresReset());
        CHECK_EQ(f.commands.errors, 1u);
        CHECK_EQ(f.commands.hellos, 0u);
        CHECK_EQ(f.commands.modeCalls, 0u);
        CHECK_EQ(f.commands.acks, 0u);
        Fixture fresh; // new receiver and byte stream model firmware reset
        fresh.byte('H'); fresh.frame(sizes[0]); fresh.payload(sizes[0]);
        CHECK_EQ(fresh.commands.hellos, 1u);
        CHECK_EQ(fresh.commands.acks, 1u);
    }
}

PICOTTL_TEST(hostImageRepeatedMalformedInputHasBoundedEffects) {
    Fixture f;
    f.frame(UINT32_MAX);
    for (unsigned i = 0; i < 100'000; ++i) { f.byte('H'); f.byte('M'); f.byte('F'); }
    CHECK_EQ(f.commands.errors, 1u);
    CHECK_EQ(f.commands.hellos, 0u);
    CHECK_EQ(f.commands.modeCalls, 0u);
    CHECK_EQ(f.commands.acks, 0u);
    CHECK_EQ(f.bytes[1], 0x5A);
    f.guards();
}

PICOTTL_TEST(hostImageIdleAndCompletedTransfersAllowCleanReconnect) {
    Fixture f;
    f.advance(timeout * 10, false);
    f.byte('H');
    f.advance(timeout * 10);
    CHECK(!f.receiver.requiresReset());
    f.frame(sizes[0]); f.payload(sizes[0]);
    f.advance(1, false); f.byte('H');
    CHECK(!f.receiver.requiresReset());
    CHECK_EQ(f.commands.hellos, 2u);
    CHECK_EQ(f.commands.acks, 1u);
}
