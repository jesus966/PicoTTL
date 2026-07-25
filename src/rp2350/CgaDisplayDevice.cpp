// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// CgaDisplayDevice.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/rp2350/CgaDisplayDevice.hpp"

namespace picottl::rp2350 {

namespace {

/// Timing lookup for the modes this CGA-family backend supports.
/// Adding a future CGA-family mode means adding a case here and, if
/// its raster is larger, growing kTimingWords. Nothing else changes.
constexpr const VideoTiming* cgaModeTiming(VideoMode mode) {
    switch (mode) {
        case VideoMode::IbmCga640:
            return &modes::kIbmCga640;
        case VideoMode::IbmCga320:
            return &modes::kIbmCga320;
        default:
            return nullptr; // Not a mode of the CGA family.
    }
}

PioScanlineEngine::Config engineConfigFrom(const CgaDisplayDevice::Config& config,
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
    // pin is the GPIO of pixel value bit 0 (Blue).
    engineConfig.videoPin = config.bluePin;
    engineConfig.timingBuffer = timingTable;
    engineConfig.timingBufferWords = timingTableWords;
    return engineConfig;
}

} // namespace

CgaDisplayDevice::CgaDisplayDevice(const Config& config)
    : engine_(engineConfigFrom(config, timingTable_, kTimingWords)),
      config_(config),
      framebuffer_(config.framebuffer) {}

bool CgaDisplayDevice::supports(VideoMode mode) const {
    return cgaModeTiming(mode) != nullptr;
}

bool CgaDisplayDevice::supportsPixelFormat(PixelFormat format) const {
    // CGA signal set: up to four RGBI wires. Narrower formats drive
    // fewer wires (Mono1: Blue; Packed2: Blue+Green) - a backend
    // capability that falls out of the architecture, not a limit.
    return format == PixelFormat::Mono1 || format == PixelFormat::Packed2 ||
           format == PixelFormat::Packed4;
}

bool CgaDisplayDevice::begin(VideoMode mode) {
    const VideoTiming* timing = cgaModeTiming(mode);
    if (timing == nullptr) {
        return false;
    }
    if (active_) {
        // Already running: report success only for the mode that is
        // actually active. Switching modes requires end() first.
        return timing == activeTiming_;
    }
    // RP2350 implementation detail (not an architectural rule): the PIO
    // output mapping requires consecutive ascending GPIOs, so VSYNC
    // must directly follow HSYNC. The public Config still names every
    // signal independently; unrealizable assignments fail here.
    if (config_.vsyncPin != static_cast<std::uint8_t>(config_.hsyncPin + 1u)) {
        return false;
    }
    if (framebuffer_ != nullptr) {
        const PixelFormat format = framebuffer_->pixelFormat();
        if (!supportsPixelFormat(format)) {
            return false;
        }
        // Same RP2350 detail for the color wires: the active format's
        // pins must form a consecutive ascending run in pixel-value
        // bit order (B, G, R, I from bluePin upward).
        const std::uint8_t colorPins[] = {config_.bluePin, config_.greenPin,
                                          config_.redPin, config_.intensityPin};
        const unsigned bits = bitsPerPixel(format);
        for (unsigned bit = 1; bit < bits; ++bit) {
            if (colorPins[bit] !=
                static_cast<std::uint8_t>(config_.bluePin + bit)) {
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

void CgaDisplayDevice::end() {
    if (!active_) {
        return;
    }
    engine_.stop();
    active_ = false;
}

bool CgaDisplayDevice::isActive() const {
    return active_;
}

std::uint16_t CgaDisplayDevice::width() const {
    return activeTiming_->hActive;
}

std::uint16_t CgaDisplayDevice::height() const {
    return activeTiming_->vActive;
}

std::uint32_t CgaDisplayDevice::refreshRateMilliHz() const {
    return activeTiming_->refreshRateMilliHz();
}

DisplaySurface* CgaDisplayDevice::surface() {
    return framebuffer_;
}

bool CgaDisplayDevice::setFramebuffer(Framebuffer* framebuffer) {
    if (active_) {
        return false;
    }
    framebuffer_ = framebuffer;
    return true;
}

} // namespace picottl::rp2350
