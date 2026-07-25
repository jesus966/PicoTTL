// PicoTTL CRT Diagnostics - pattern framework
//
// OFFICIAL PicoTTL APPLICATION - the project's permanent hardware and
// framework validation suite, not a development experiment. Its output
// is part of the project's regression surface: whenever a backend
// changes, this application must still produce identical expected
// output on the same monitor. Every present and future backend (MDA,
// CGA, EGA, Amstrad, host) is validated with this same
// application before higher-level APIs are exercised.
//
// The pattern API is completely backend independent: patterns query
// live framework objects, never assume any geometry (all sizes come
// from the Display API, never from mode constants) and never assume
// color meaning (all colors come from the palette). Adding a diagnostic
// requires exactly one new class.
//
// Capability policy: patterns must branch on QUERIED capabilities
// (palette, pixel format, geometry - and, when future backends need it,
// additive DisplayDevice capability queries such as intensity support
// or pixel-output count), never on backend identity. No
// backend-specific conditionals belong in this application.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "picottl/PicoTTL.hpp"

namespace picottl::diagnostics {

/// Backend-appropriate colors for diagnostic patterns.
///
/// Patterns never assume what a Color index means; this palette is
/// derived from the active framebuffer's pixel format. (A future
/// DisplayDevice-provided palette can replace this helper additively.)
struct DiagnosticPalette {
    Color black{0};
    Color dim{0};    ///< Lowest distinct level (INTENSITY only on MDA).
    Color normal{1};
    Color bright{1}; ///< Brightest available level.
};

/// Palette for a given pixel format.
constexpr DiagnosticPalette paletteFor(PixelFormat format) {
    switch (format) {
        case PixelFormat::Mono1:
            return {Color{0}, Color{1}, Color{1}, Color{1}};
        case PixelFormat::Packed2:
            // MDA mapping: bit 0 = VIDEO, bit 1 = INTENSITY.
            return {Color{0}, Color{2}, Color{1}, Color{3}};
        case PixelFormat::Packed4:
            // RGBI mapping (CGA): dark gray, light gray, white - the
            // format's monochrome-appropriate levels, so the generic
            // patterns render neutrally in color modes.
            return {Color{0}, Color{8}, Color{7}, Color{15}};
        default: {
            const auto all = static_cast<std::uint8_t>(
                (1u << bitsPerPixel(format)) - 1u);
            return {Color{0}, Color{1}, Color{1}, Color{all}};
        }
    }
}

/// Human-readable name of a video mode (diagnostics presentation only).
constexpr const char* modeName(VideoMode mode) {
    switch (mode) {
        case VideoMode::IbmMda:
            return "IBM MDA 100% compatible";
        case VideoMode::MdaOverscan:
            return "MDA overscan (constraint-derived)";
        case VideoMode::IbmCga640:
            return "IBM CGA 640x200";
        case VideoMode::IbmCga320:
            return "IBM CGA 320x200";
        case VideoMode::IbmEga350:
            return "IBM EGA 640x350";
    }
    return "unknown";
}

/// Human-readable name of a pixel format.
constexpr const char* pixelFormatName(PixelFormat format) {
    switch (format) {
        case PixelFormat::Mono1:
            return "Mono1 (1 bpp)";
        case PixelFormat::Packed2:
            return "Packed2 (2 bpp)";
        case PixelFormat::Packed4:
            return "Packed4 (4 bpp)";
        case PixelFormat::Packed6:
            return "Packed6 (6 bpp)";
        case PixelFormat::Packed8:
            return "Packed8 (8 bpp)";
    }
    return "unknown";
}

/// Everything a pattern may query: live framework objects rather than
/// preformatted strings, so the context stays extensible.
struct DiagnosticContext {
    Display& display;
    Graphics& graphics;
    Text& text;
    DiagnosticPalette palette;
    VideoMode mode;
    Framebuffer* framebuffer;  ///< Active pixel storage (may be null).
    const char* backendName;   ///< Presentation name of the backend.
};

/// One diagnostic pattern. Adding a diagnostic = one new class.
class DiagnosticPattern {
public:
    virtual ~DiagnosticPattern() = default;

    /// Short name shown by the application.
    virtual const char* name() const = 0;

    /// Draws the full pattern. Called on entry and after mode changes;
    /// must derive all geometry from the context.
    virtual void draw(DiagnosticContext& context) = 0;

    /// Optional animation hook (monotonic milliseconds).
    virtual void update(DiagnosticContext& context, std::uint32_t nowMs) {
        (void)context;
        (void)nowMs;
    }

    /// Optional button hook: return true to consume the press; on false
    /// the application advances to the next pattern.
    virtual bool onButtonPress(DiagnosticContext& context) {
        (void)context;
        return false;
    }

    /// Optional LONG-press hook (button held ~1.5 s). Same contract as
    /// onButtonPress: return true to consume, false to advance. The
    /// default (false) gives every pattern a universal "hold to move
    /// on" escape; patterns that consume short presses for navigation
    /// (e.g. mode selection) override this as their "activate" action.
    virtual bool onButtonLongPress(DiagnosticContext& context) {
        (void)context;
        return false;
    }
};

} // namespace picottl::diagnostics
