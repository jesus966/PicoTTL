// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Display.hpp
// The central application-facing abstraction of PicoTTL.
//
// PUBLIC API - FROZEN. Future functionality (graphics, text, framebuffer
// access) is added as new members; existing signatures never change.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "picottl/DisplayDevice.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/VideoMode.hpp"

namespace picottl {

/// A TTL display.
///
/// Applications think exclusively in terms of this class: displays,
/// video modes, graphics, text and framebuffers. All hardware concerns
/// live inside the injected DisplayDevice backend.
///
/// Usage:
///   picottl::rp2350::MdaDisplayDevice device(config);
///   picottl::Display display(device);
///   display.begin(picottl::VideoMode::IbmMda);
///
/// Switching backends (MDA, CGA, EGA, the host test device) only
/// changes the device construction line; all Display-based code
/// remains unchanged.
class Display {
public:
    /// @param device Backend that produces the video signal. Must outlive
    ///               this Display.
    explicit Display(DisplayDevice& device)
        : device_(device), graphics_(device.surface()) {}

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    /// True if the underlying backend can output the given mode.
    bool supports(VideoMode mode) const {
        return device_.supports(mode);
    }

    /// True if the underlying backend can scan out framebuffers of the
    /// given pixel format.
    bool supportsPixelFormat(PixelFormat format) const {
        return device_.supportsPixelFormat(format);
    }

    /// Starts video output in the given mode. Returns false if the mode
    /// is unsupported, hardware resources are unavailable, or a
    /// different mode is already active (call end() first). On success
    /// the graphics context is rebound to the device's current surface.
    bool begin(VideoMode mode) {
        if (!device_.begin(mode)) {
            return false;
        }
        mode_ = mode;
        graphics_ = Graphics(device_.surface()); // track the active surface
        return true;
    }

    /// Stops video output.
    void end() {
        device_.end();
    }

    /// True while video output is active.
    bool isActive() const {
        return device_.isActive();
    }

    /// The current video mode. Valid after a successful begin().
    VideoMode mode() const {
        return mode_;
    }

    /// Visible width in pixels. Valid after a successful begin().
    std::uint16_t width() const {
        return device_.width();
    }

    /// Visible height in pixels. Valid after a successful begin().
    std::uint16_t height() const {
        return device_.height();
    }

    /// Vertical refresh rate in millihertz. Valid after a successful begin().
    std::uint32_t refreshRateMilliHz() const {
        return device_.refreshRateMilliHz();
    }

    /// The drawing surface of this display, or nullptr if the backend
    /// was configured without one. Drawing may happen at any time; the
    /// surface contents are scanned out continuously while active.
    DisplaySurface* surface() {
        return device_.surface();
    }

    /// Rebinds the pixel storage while output is inactive (end()
    /// first). Mode changes and framebuffer changes are independent
    /// operations; the next begin() validates the combination and
    /// rebinds graphics() to the new surface.
    bool setFramebuffer(Framebuffer* framebuffer) {
        return device_.setFramebuffer(framebuffer);
    }

    /// Drawing primitives over this display's surface. When the backend
    /// has no surface, all drawing operations are no-ops. Geometry is
    /// always derived live from the bound surface - Graphics caches
    /// nothing.
    Graphics& graphics() {
        return graphics_;
    }

private:
    DisplayDevice& device_;
    Graphics graphics_;
    VideoMode mode_ = VideoMode::IbmMda;
};

} // namespace picottl
