// PicoTTL - backend lifecycle test support (no hardware emulation).
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>

namespace picottl_test {

// Tests construct one backend at a time. This records the engine boundary,
// not hardware registers, DMA transfers, pin levels or signal timing.
struct EngineRecording {
    const std::uint8_t* source = nullptr;
    std::size_t sourceBytes = 0;
    std::uint8_t bitsPerPixel = 1;
    std::uint8_t pinCount = 1;
    const std::uint8_t* configuredSource = nullptr;
    const std::uint8_t* startedSource = nullptr;
    unsigned starts = 0;
    bool running = false;
};

const EngineRecording& engineRecording();

} // namespace picottl_test
