// PicoTTL showcase demonstrations - shared drawing utilities.
//
// Filled shapes, aspect-corrected ellipses, ordered (Bayer) dithering,
// an integer sine, and the era-authentic full-screen transitions used
// by the official demonstration programs (examples 96-98).
//
// Everything here is APPLICATION-LEVEL code built strictly on the
// public PicoTTL API (Graphics primitives only). Colors are passed
// through opaquely - these helpers assume nothing about what a pixel
// value means (design principle 20); the demos supply backend-scoped
// colors.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "picottl/Color.hpp"
#include "picottl/Graphics.hpp"

namespace picottl::demo {

/// Fills an axis-aligned rectangle (clipped by Graphics).
void fillRect(Graphics& g, std::int32_t x, std::int32_t y,
              std::int32_t width, std::int32_t height, Color color);

/// Draws a 1-pixel rectangle outline.
void drawRect(Graphics& g, std::int32_t x, std::int32_t y,
              std::int32_t width, std::int32_t height, Color color);

/// Draws an ellipse outline centered at (cx, cy) with pixel radii
/// (rx, ry). Pass rx == ry for a circle in pixel space; the demos pick
/// radii that compensate their mode's pixel aspect ratio so shapes
/// look round on the actual monitor.
void drawEllipse(Graphics& g, std::int32_t cx, std::int32_t cy,
                 std::int32_t rx, std::int32_t ry, Color color);

/// Fills an ellipse (row spans; exact integer math).
void fillEllipse(Graphics& g, std::int32_t cx, std::int32_t cy,
                 std::int32_t rx, std::int32_t ry, Color color);

/// 8x8 ordered-dither (Bayer) threshold for a pixel: 0..63. The
/// classic period technique for simulating intermediate tones with a
/// fixed palette - deterministic, so redrawn areas do not shimmer.
std::uint8_t bayer64(std::int32_t x, std::int32_t y);

/// Fills a rectangle with an ordered-dither mix of two colors.
/// level = 0 gives pure `a`, 64 pure `b`, 32 a 50% checkered mix.
void ditherRect(Graphics& g, std::int32_t x, std::int32_t y,
                std::int32_t width, std::int32_t height,
                Color a, Color b, std::int32_t level);

/// Fills a rectangle with a smooth VERTICAL gradient across a ramp of
/// colors (ramp[0] at the top, ramp[count-1] at the bottom), blending
/// adjacent ramp entries with ordered dithering.
void ditherVGradient(Graphics& g, std::int32_t x, std::int32_t y,
                     std::int32_t width, std::int32_t height,
                     const Color* ramp, std::int32_t count);

/// Fills a rectangle with a smooth HORIZONTAL gradient across a ramp
/// (ramp[0] at the left, ramp[count-1] at the right).
void ditherHGradient(Graphics& g, std::int32_t x, std::int32_t y,
                     std::int32_t width, std::int32_t height,
                     const Color* ramp, std::int32_t count);

/// Integer sine: angle 0..255 covers one full turn; returns -128..128.
/// Parabolic approximation - smooth enough for demo motion, no tables,
/// no floating point.
std::int32_t isin(std::uint32_t angle);

/// Integer cosine companion of isin().
inline std::int32_t icos(std::uint32_t angle) { return isin(angle + 64u); }

/// Integer square root (exact floor) of a 64-bit value - handy for
/// distance-based shading effects.
std::uint32_t isqrt64(std::uint64_t value);

// ---------------------------------------------------------------------------
// Full-screen transitions (blocking; they pace themselves with sleep_ms).
// Both were staples of 1980s software: the wipe of menu-driven programs
// and demonstration disks, and the pseudo-random pixel dissolve
// ("fizzle fade") popularized on the IBM PC by late-DOS titles.
// ---------------------------------------------------------------------------

/// Wipes the whole surface to `color`, top to bottom, in horizontal
/// bands of `bandHeight` pixels every `stepDelayMs` milliseconds.
void wipeDown(Graphics& g, Color color, std::int32_t bandHeight,
              std::uint32_t stepDelayMs);

/// Dissolves the whole surface to `color` pixel by pixel in
/// pseudo-random order (18-bit maximal LFSR - visits every pixel
/// exactly once), paced to roughly `durationMs` milliseconds.
void fizzleFade(Graphics& g, Color color, std::uint32_t durationMs);

} // namespace picottl::demo
