// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// DisplayDevice.hpp
// Backend interface implemented by every display backend.
//
// PUBLIC API (SPI side) - applications only pass a DisplayDevice to
// picottl::Display; they never call it directly. Backend implementers
// (MDA, CGA, EGA, host - and any future standard) derive from this
// class. Methods may be ADDED over time; existing signatures are
// frozen.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "picottl/PixelFormat.hpp"
#include "picottl/VideoMode.hpp"

namespace picottl {

class DisplaySurface;
class Framebuffer;

/// Abstract display backend (one implementation per video standard).
///
/// Owns all standard-specific knowledge: signal generation, mode
/// geometry and, in future steps, pixel encoding. Hardware details
/// never cross this boundary upward.
class DisplayDevice {
public:
    virtual ~DisplayDevice() = default;

    DisplayDevice(const DisplayDevice&) = delete;
    DisplayDevice& operator=(const DisplayDevice&) = delete;

    /// True if this backend can output the given mode.
    virtual bool supports(VideoMode mode) const = 0;

    /// True if this backend can scan out framebuffers of the given
    /// pixel format. Backend capabilities are implementation details:
    /// other backends may support different sets without any framework
    /// change.
    virtual bool supportsPixelFormat(PixelFormat format) const = 0;

    /// Configures and starts video output in the given mode.
    /// Returns false if the mode is unsupported, resources are
    /// unavailable, or a different mode is already active (call end()
    /// first to switch modes).
    virtual bool begin(VideoMode mode) = 0;

    /// Stops video output.
    virtual void end() = 0;

    /// True while video output is active.
    virtual bool isActive() const = 0;

    /// Visible width in pixels of the current (or default) mode.
    virtual std::uint16_t width() const = 0;

    /// Visible height in pixels of the current (or default) mode.
    virtual std::uint16_t height() const = 0;

    /// Vertical refresh rate in millihertz of the current (or default) mode.
    virtual std::uint32_t refreshRateMilliHz() const = 0;

    /// The drawing surface scanned out by this backend, or nullptr when
    /// the backend was configured without one.
    virtual DisplaySurface* surface() = 0;

    /// Rebinds the pixel storage while output is inactive (end()
    /// first); the next begin() validates the combination. A GENERIC
    /// framework capability: every backend supports it. Video modes and
    /// framebuffers are deliberately independent concerns - a mode
    /// describes the generated raster, a framebuffer is only the memory
    /// that produces pixels. Returns false while output is active.
    virtual bool setFramebuffer(Framebuffer* framebuffer) = 0;

protected:
    DisplayDevice() = default;
};

} // namespace picottl
