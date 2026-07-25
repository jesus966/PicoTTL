// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// host/HostDisplayDevice.hpp
// Display backend without hardware, for desktop hosts.
//
// PUBLIC API - applications and tests construct it and hand it to
// picottl::Display exactly like a hardware backend.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "picottl/DisplayDevice.hpp"
#include "picottl/Framebuffer.hpp"
#include "picottl/video/ModeTimings.hpp" // backend SPI

namespace picottl::host {

/// Display backend whose "scan-out" is simply the attached framebuffer
/// memory. No video signal is generated: it exists for automated
/// regression tests, rendering validation, framebuffer inspection and
/// font/bitmap debugging on desktop hosts (Windows/Linux/macOS).
///
/// Capabilities: every catalog VideoMode and every PixelFormat are
/// supported - this backend's "electrical interface" is memory. A
/// concrete demonstration that backend limitations are implementation
/// details, never architectural ones.
///
/// Usage (identical shape to a hardware backend):
///   picottl::host::HostDisplayDevice::Config config;
///   config.framebuffer = &framebuffer;
///   picottl::host::HostDisplayDevice device(config);
///   picottl::Display display(device);
///   display.begin(picottl::VideoMode::IbmMda);
class HostDisplayDevice final : public DisplayDevice {
public:
    struct Config {
        /// Pixel storage of any format, or nullptr for a surface-less
        /// device. Caller-owned; must match the mode's visible size at
        /// begin().
        Framebuffer* framebuffer = nullptr;
    };

    /// The configuration is copied; it may be destroyed after construction.
    explicit HostDisplayDevice(const Config& config);

    bool supports(VideoMode mode) const override;
    bool supportsPixelFormat(PixelFormat format) const override;
    bool begin(VideoMode mode) override;
    void end() override;
    bool isActive() const override;
    std::uint16_t width() const override;
    std::uint16_t height() const override;
    std::uint32_t refreshRateMilliHz() const override;
    DisplaySurface* surface() override;

    /// Replaces the framebuffer while output is inactive; validated by
    /// the next begin(). Mirrors the hardware backends' contract.
    bool setFramebuffer(Framebuffer* framebuffer) override;

private:
    Framebuffer* framebuffer_ = nullptr;
    const VideoTiming* activeTiming_ = &modes::kIbmMda; ///< Selected mode's timing.
    bool active_ = false;
};

} // namespace picottl::host
