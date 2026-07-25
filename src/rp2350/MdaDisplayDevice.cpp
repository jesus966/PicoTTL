// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// MdaDisplayDevice.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/rp2350/MdaDisplayDevice.hpp"

namespace picottl::rp2350 {

namespace {

/// Timing lookup for the modes this MDA-family backend supports.
/// Adding a future MDA-family mode means adding a case here and, if
/// its raster is larger, growing kTimingWords. Nothing else changes.
constexpr const VideoTiming* mdaModeTiming(VideoMode mode) {
    switch (mode) {
        case VideoMode::IbmMda:
            return &modes::kIbmMda;
        case VideoMode::MdaOverscan:
            return &modes::kMdaOverscan;
        default:
            return nullptr; // Not a mode of the MDA family.
    }
}

PioScanlineEngine::Config engineConfigFrom(const MdaDisplayDevice::Config& config,
                                           std::uint32_t* timingTable,
                                           std::size_t timingTableWords) {
    PioScanlineEngine::Config engineConfig;
    engineConfig.pioInstance = config.pioInstance;
    engineConfig.stateMachine = config.stateMachine;
    engineConfig.pixelStateMachine = config.pixelStateMachine;
    engineConfig.dmaDataChannel = config.dmaDataChannel;
    engineConfig.dmaControlChannel = config.dmaControlChannel;
    engineConfig.pixelDmaDataChannel = config.pixelDmaDataChannel;
    engineConfig.pixelDmaControlChannel = config.pixelDmaControlChannel;
    engineConfig.hsyncPin = config.hsyncPin;
    // The engine implements the consecutive PIO pin mapping; its base
    // pin is the GPIO of pixel value bit 0.
    engineConfig.videoPin = config.pixelPins[0] >= 0
                                ? static_cast<std::uint8_t>(config.pixelPins[0])
                                : config.videoPin;
    engineConfig.timingBuffer = timingTable;
    engineConfig.timingBufferWords = timingTableWords;
    return engineConfig;
}

} // namespace

MdaDisplayDevice::MdaDisplayDevice(const Config& config)
    : engine_(engineConfigFrom(config, timingTable_, kTimingWords)),
      framebuffer_(config.framebuffer) {
    // Resolve the effective GPIO of every pixel-value bit: explicit
    // entries win, unset entries fall back to the videoPin shorthand.
    for (std::size_t bit = 0; bit < Config::kMaxPixelBits; ++bit) {
        pixelPins_[bit] =
            config.pixelPins[bit] >= 0
                ? config.pixelPins[bit]
                : static_cast<std::int16_t>(config.videoPin + bit);
    }
}

bool MdaDisplayDevice::supports(VideoMode mode) const {
    return mdaModeTiming(mode) != nullptr;
}

bool MdaDisplayDevice::supportsPixelFormat(PixelFormat format) const {
    // MDA signal set: pixel value bit 0 -> VIDEO, bit 1 -> INTENSITY.
    // A backend capability, not an architectural limit.
    return format == PixelFormat::Mono1 || format == PixelFormat::Packed2;
}

bool MdaDisplayDevice::begin(VideoMode mode) {
    const VideoTiming* timing = mdaModeTiming(mode);
    if (timing == nullptr) {
        return false;
    }
    if (active_) {
        // Already running: report success only for the mode that is
        // actually active. Switching modes requires end() first.
        return timing == activeTiming_;
    }
    if (framebuffer_ != nullptr) {
        const PixelFormat format = framebuffer_->pixelFormat();
        if (!supportsPixelFormat(format)) {
            return false;
        }
        // RP2350 implementation detail (not an architectural rule): the
        // PIO output mapping requires the active format's pixel pins to
        // be consecutive ascending GPIOs. Arbitrary mappings fail here
        // explicitly.
        const unsigned bits = bitsPerPixel(format);
        for (unsigned bit = 1; bit < bits; ++bit) {
            if (pixelPins_[bit] !=
                static_cast<std::int16_t>(pixelPins_[0] + bit)) {
                return false;
            }
        }
        if (framebuffer_->width() != timing->hActive ||
            framebuffer_->height() != timing->vActive) {
            return false;
        }
        if (!engine_.setPixelSource(
                framebuffer_->data(), framebuffer_->sizeBytes(),
                static_cast<std::uint8_t>(bitsPerPixel(format)))) {
            return false;
        }
    }
    if (!engine_.configure(*timing)) {
        return false;
    }
    if (!engine_.start()) {
        return false;
    }
    activeTiming_ = timing;
    active_ = true;
    return true;
}

void MdaDisplayDevice::end() {
    if (!active_) {
        return;
    }
    engine_.stop();
    active_ = false;
}

bool MdaDisplayDevice::isActive() const {
    return active_;
}

std::uint16_t MdaDisplayDevice::width() const {
    return activeTiming_->hActive;
}

std::uint16_t MdaDisplayDevice::height() const {
    return activeTiming_->vActive;
}

std::uint32_t MdaDisplayDevice::refreshRateMilliHz() const {
    return activeTiming_->refreshRateMilliHz();
}

DisplaySurface* MdaDisplayDevice::surface() {
    return framebuffer_;
}

bool MdaDisplayDevice::setFramebuffer(Framebuffer* framebuffer) {
    if (active_) {
        return false;
    }
    framebuffer_ = framebuffer;
    return true;
}

} // namespace picottl::rp2350
