// PicoTTL - recording substitute for PioScanlineEngine in host tests.
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "support/RecordingScanlineEngine.hpp"

#include "picottl/rp2350/PioScanlineEngine.hpp"

namespace {
picottl_test::EngineRecording recording;
}

namespace picottl_test {

const EngineRecording& engineRecording() {
    return recording;
}

} // namespace picottl_test

namespace picottl::rp2350 {

PioScanlineEngine::PioScanlineEngine(const Config& config)
    : config_(config) {
    recording = {};
}

PioScanlineEngine::~PioScanlineEngine() {
    stop();
}

bool PioScanlineEngine::setPixelSource(const std::uint8_t* data,
                                      std::size_t sizeBytes,
                                      std::uint8_t bitsPerPixel,
                                      std::uint8_t pinCount) {
    if (running_) {
        return false;
    }
    // Model only the source-binding contract used by the backends.
    // Production alignment, packing and hardware setup are not tested here.
    pixelSource_ = data;
    pixelSourceBytes_ = data != nullptr ? sizeBytes : 0;
    pixelBitsPerPixel_ = data != nullptr ? bitsPerPixel : 1;
    pixelPinCount_ = data != nullptr
                         ? (pinCount != 0 ? pinCount : bitsPerPixel)
                         : 1;
    configured_ = false;
    recording.source = pixelSource_;
    recording.sourceBytes = pixelSourceBytes_;
    recording.bitsPerPixel = pixelBitsPerPixel_;
    recording.pinCount = pixelPinCount_;
    return true;
}

bool PioScanlineEngine::configure(const VideoTiming& timing) {
    timing_ = timing;
    recording.configuredSource = pixelSource_;
    configured_ = true;
    return true;
}

bool PioScanlineEngine::start() {
    if (!configured_) {
        return false;
    }
    recording.startedSource = pixelSource_;
    ++recording.starts;
    recording.running = running_ = true;
    return true;
}

void PioScanlineEngine::stop() {
    recording.running = running_ = false;
}

bool PioScanlineEngine::isRunning() const {
    return running_;
}

} // namespace picottl::rp2350
