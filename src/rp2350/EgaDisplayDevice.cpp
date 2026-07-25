// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// EgaDisplayDevice.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/rp2350/EgaDisplayDevice.hpp"

namespace picottl::rp2350 {

namespace {

/// Timing lookup for the modes this EGA-family backend supports. The
/// 200-line entries are the CGA catalog timings, referenced directly:
/// IBM EGA is hardware-compatible with CGA timings (compatibility, not
/// duplication). Adding a future EGA-family mode means adding a case
/// here and, if its raster is larger, growing kTimingWords. Nothing
/// else changes.
constexpr const VideoTiming* egaModeTiming(VideoMode mode) {
    switch (mode) {
        case VideoMode::IbmEga350:
            return &modes::kIbmEga350;
        case VideoMode::IbmCga640:
            return &modes::kIbmCga640;
        case VideoMode::IbmCga320:
            return &modes::kIbmCga320;
        default:
            return nullptr; // Not a mode this backend can generate.
    }
}

PioScanlineEngine::Config engineConfigFrom(const EgaDisplayDevice::Config& config,
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
    // pin is the GPIO of pixel value bit 0 (Primary Blue).
    engineConfig.videoPin = config.primaryBluePin;
    engineConfig.timingBuffer = timingTable;
    engineConfig.timingBufferWords = timingTableWords;
    return engineConfig;
}

} // namespace

EgaDisplayDevice::EgaDisplayDevice(const Config& config)
    : engine_(engineConfigFrom(config, timingTable_, kTimingWords)),
      config_(config),
      framebuffer_(config.framebuffer) {}

bool EgaDisplayDevice::supports(VideoMode mode) const {
    return egaModeTiming(mode) != nullptr;
}

bool EgaDisplayDevice::supportsPixelFormat(PixelFormat format) const {
    // EGA signal set: up to six rgbRGB wires, fed by a stream whose
    // depth divides 32 (RP2350 PIO OSR granularity - see the header).
    // Packed8 carries the 6-bit EGA value per byte; narrower formats
    // drive fewer wires. Packed6 is NOT scannable on this platform.
    return format == PixelFormat::Mono1 || format == PixelFormat::Packed2 ||
           format == PixelFormat::Packed4 || format == PixelFormat::Packed8;
}

bool EgaDisplayDevice::begin(VideoMode mode) {
    const VideoTiming* timing = egaModeTiming(mode);
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
        // bit order (B, G, R, b, g, r from primaryBluePin upward).
        // Packed8 streams 8 bits per pixel but drives only the six
        // wires; narrower formats drive their own bit count.
        const std::uint8_t colorPins[] = {
            config_.primaryBluePin,   config_.primaryGreenPin,
            config_.primaryRedPin,    config_.secondaryBluePin,
            config_.secondaryGreenPin, config_.secondaryRedPin};
        const unsigned wires =
            format == PixelFormat::Packed8 ? 6u : bitsPerPixel(format);
        for (unsigned bit = 1; bit < wires; ++bit) {
            if (colorPins[bit] !=
                static_cast<std::uint8_t>(config_.primaryBluePin + bit)) {
                return false;
            }
        }
        if (framebuffer_->width() != timing->hActive ||
            framebuffer_->height() != timing->vActive) {
            return false;
        }
        if (!engine_.setPixelSource(
                framebuffer_->data(), framebuffer_->sizeBytes(),
                static_cast<std::uint8_t>(bitsPerPixel(format)),
                static_cast<std::uint8_t>(wires))) {
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

void EgaDisplayDevice::end() {
    if (!active_) {
        return;
    }
    engine_.stop();
    active_ = false;
}

bool EgaDisplayDevice::isActive() const {
    return active_;
}

std::uint16_t EgaDisplayDevice::width() const {
    return activeTiming_->hActive;
}

std::uint16_t EgaDisplayDevice::height() const {
    return activeTiming_->vActive;
}

std::uint32_t EgaDisplayDevice::refreshRateMilliHz() const {
    return activeTiming_->refreshRateMilliHz();
}

DisplaySurface* EgaDisplayDevice::surface() {
    return framebuffer_;
}

bool EgaDisplayDevice::setFramebuffer(Framebuffer* framebuffer) {
    if (active_) {
        return false;
    }
    framebuffer_ = framebuffer;
    return true;
}

} // namespace picottl::rp2350
