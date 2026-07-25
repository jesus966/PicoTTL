// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Framebuffer.hpp
// Generic pixel memory abstraction.
//
// PUBLIC API - FROZEN. Platform-independent.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>

#include "picottl/DisplaySurface.hpp"
#include "picottl/PixelFormat.hpp"

namespace picottl {

/// A DisplaySurface whose pixels live in addressable memory.
///
/// This is a generic memory abstraction: it describes WHAT is stored
/// (format, location, size), never what the values mean electrically or
/// which video standard consumes them. Rendering code (Graphics, Text)
/// keeps seeing only DisplaySurface; display backends accept any
/// Framebuffer and validate pixelFormat() at runtime - they never
/// require concrete framebuffer classes.
class Framebuffer : public DisplaySurface {
public:
    /// Storage pixel format.
    virtual PixelFormat pixelFormat() const = 0;

    /// Raw pixel storage (for backends, tests and inspection).
    virtual const std::uint8_t* data() const = 0;

    /// Total bytes of pixel storage.
    virtual std::size_t sizeBytes() const = 0;

protected:
    Framebuffer() = default;
};

} // namespace picottl
