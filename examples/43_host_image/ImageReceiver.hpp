// Copyright (c) 2026 Jesus Fernandez Gamito
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace host_image {

enum class ReceiveError : std::uint8_t { Length, Timeout, Disconnected };

// Example-private byte-stream receiver. Transport/time are supplied by the
// caller; Commands provides the existing mode operations and wire replies.
// After ambiguous input, only reconstructing the receiver on firmware reset
// establishes a new session. DTR/reconnect and command-looking bytes cannot.
template <class Commands>
class ImageReceiver {
public:
    static constexpr std::uint32_t kInactivityMs = 10'000;

    ImageReceiver(std::uint8_t* storage, std::size_t capacity, Commands& commands)
        : storage_(storage), capacity_(capacity), commands_(commands) {}

    // One byte (0..255) or a negative no-data result. Bounded work per poll,
    // apart from clearing at most one validated framebuffer on payload failure.
    void poll(int byte, std::uint32_t nowMs, bool connected) {
        if (state_ == State::Fault) return;
        if (!connected) {
            if (state_ != State::Command) fail(ReceiveError::Disconnected);
            return;
        }
        if (state_ != State::Command &&
            static_cast<std::uint32_t>(nowMs - lastByteMs_) >= kInactivityMs) {
            fail(ReceiveError::Timeout);
            return;
        }
        if (byte < 0) return;
        lastByteMs_ = nowMs;
        const auto value = static_cast<std::uint8_t>(byte);
        switch (state_) {
            case State::Command:
                if (value == 'H') commands_.hello();
                else if (value == 'M') state_ = State::Mode;
                else if (value == 'F') {
                    length_ = 0;
                    headerBytes_ = 0;
                    state_ = State::Length;
                }
                break;
            case State::Mode:
                state_ = State::Command;
                commands_.modeResult(commands_.setMode(value));
                break;
            case State::Length:
                length_ |= static_cast<std::uint32_t>(value) << (8u * headerBytes_);
                if (++headerBytes_ == 4) {
                    expected_ = commands_.frameBytes();
                    if (length_ == 0) {
                        // No declared body remains: this boundary is known.
                        state_ = State::Command;
                        commands_.error(ReceiveError::Length, expected_);
                    } else if (storage_ == nullptr || length_ > capacity_ || length_ != expected_) {
                        fail(ReceiveError::Length);
                    } else {
                        received_ = 0;
                        state_ = State::Payload;
                    }
                }
                break;
            case State::Payload:
                storage_[received_++] = value;
                if (received_ == length_) {
                    state_ = State::Command;
                    commands_.frameAccepted();
                }
                break;
            case State::Fault:
                break;
        }
    }

    bool requiresReset() const { return state_ == State::Fault; }

private:
    enum class State : std::uint8_t { Command, Mode, Length, Payload, Fault };

    void fail(ReceiveError error) {
        // Reception is intentionally live/top-down, not double-buffered.
        // Remove the incomplete image instead of retaining a partial update.
        if (state_ == State::Payload) std::memset(storage_, 0, length_);
        state_ = State::Fault;
        commands_.error(error, expected_);
    }

    std::uint8_t* storage_;
    std::size_t capacity_;
    Commands& commands_;
    State state_ = State::Command;
    std::uint32_t lastByteMs_ = 0;
    std::uint32_t length_ = 0;
    std::uint32_t received_ = 0;
    std::uint32_t expected_ = 0;
    unsigned headerBytes_ = 0;
};

} // namespace host_image
