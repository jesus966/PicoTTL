// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// host/HostDisplayDevice.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/host/HostDisplayDevice.hpp"

namespace picottl::host {

HostDisplayDevice::HostDisplayDevice(const Config& config)
    : framebuffer_(config.framebuffer) {}

bool HostDisplayDevice::supports(VideoMode mode) const {
    // Every catalog mode: the host "hardware" is memory.
    switch (mode) {
        case VideoMode::IbmMda:
        case VideoMode::MdaOverscan:
        case VideoMode::IbmCga640:
        case VideoMode::IbmCga320:
        case VideoMode::IbmEga350:
            return true;
    }
    return false;
}

bool HostDisplayDevice::supportsPixelFormat(PixelFormat) const {
    return true; // Every format: there is no electrical interface.
}

bool HostDisplayDevice::begin(VideoMode mode) {
    if (!supports(mode)) {
        return false;
    }
    const VideoTiming* timing = &timingFor(mode);
    if (active_) {
        // Same semantics as hardware backends: success only for the
        // active mode; switching requires end() first.
        return timing == activeTiming_;
    }
    if (framebuffer_ != nullptr) {
        if (framebuffer_->width() != timing->hActive ||
            framebuffer_->height() != timing->vActive) {
            return false;
        }
    }
    activeTiming_ = timing;
    active_ = true;
    return true;
}

void HostDisplayDevice::end() {
    active_ = false;
}

bool HostDisplayDevice::isActive() const {
    return active_;
}

std::uint16_t HostDisplayDevice::width() const {
    return activeTiming_->hActive;
}

std::uint16_t HostDisplayDevice::height() const {
    return activeTiming_->vActive;
}

std::uint32_t HostDisplayDevice::refreshRateMilliHz() const {
    return activeTiming_->refreshRateMilliHz();
}

DisplaySurface* HostDisplayDevice::surface() {
    return framebuffer_;
}

bool HostDisplayDevice::setFramebuffer(Framebuffer* framebuffer) {
    if (active_) {
        return false;
    }
    framebuffer_ = framebuffer;
    return true;
}

} // namespace picottl::host
