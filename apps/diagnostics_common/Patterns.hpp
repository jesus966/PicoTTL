// PicoTTL CRT Diagnostics - pattern set
//
// REFERENCE VALIDATION PATTERNS. Every pattern documents: what it
// validates, its expected appearance, and what defects indicate - the
// on-screen output is a permanent reference for hardware and framework
// regressions on every backend.
//
// Patterns derive ALL geometry and colors from the DiagnosticContext
// (stateless by design; only interaction/animation state is allowed)
// and never assume resolution, color depth, monitor type or backend
// family. As an official framework tool, this file may consult the
// backend SPI timing catalog (timingFor) for presentation purposes;
// application code should not.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "DiagnosticPattern.hpp"

#include "picottl/video/ModeTimings.hpp" // SPI: authoritative timing catalog

namespace picottl::diagnostics {

// ---------------------------------------------------------------------------
// Information and measurement pages
// ---------------------------------------------------------------------------

/// Validates: correct mode/backend bring-up and API-reported geometry.
/// Expected: all values match the selected mode's specification.
/// Defects indicate: wrong mode selected, framebuffer mismatch, stale
/// geometry after a mode switch (values must always match the raster).
class InfoPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Info"; }

    void draw(DiagnosticContext& c) override {
        c.graphics.clear(c.palette.black);
        const VideoTiming& t = timingFor(c.mode);
        Text& txt = c.text;
        txt.setUnderline(false);
        txt.setColors(c.palette.bright, c.palette.black);
        txt.setCursor(2, 1);
        txt.printf("PicoTTL CRT Diagnostics  v%d.%d.%d", PICOTTL_VERSION_MAJOR,
                   PICOTTL_VERSION_MINOR, PICOTTL_VERSION_PATCH);
        txt.setColors(c.palette.normal, c.palette.black);
        std::int32_t row = 3;
        txt.setCursor(2, row++);
        txt.printf("Backend     : %s", c.backendName);
        txt.setCursor(2, row++);
        txt.printf("Mode        : %s", modeName(c.mode));
        txt.setCursor(2, row++);
        txt.printf("Visible     : %d x %d pixels", c.display.width(),
                   c.display.height());
        const std::uint32_t refresh = c.display.refreshRateMilliHz();
        txt.setCursor(2, row++);
        txt.printf("Refresh     : %lu.%03lu Hz",
                   static_cast<unsigned long>(refresh / 1000u),
                   static_cast<unsigned long>(refresh % 1000u));
        txt.setCursor(2, row++);
        txt.printf("Line freq   : %lu.%03lu kHz",
                   static_cast<unsigned long>(t.lineFrequencyHz() / 1000u),
                   static_cast<unsigned long>(t.lineFrequencyHz() % 1000u));
        txt.setCursor(2, row++);
        txt.printf("Pixel clock : %lu.%03lu MHz",
                   static_cast<unsigned long>(t.pixelClockHz / 1000000u),
                   static_cast<unsigned long>((t.pixelClockHz / 1000u) % 1000u));
        if (c.framebuffer != nullptr) {
            txt.setCursor(2, row++);
            txt.printf("Framebuffer : PackedFramebuffer<%u>, %lu bytes",
                       bitsPerPixel(c.framebuffer->pixelFormat()),
                       static_cast<unsigned long>(c.framebuffer->sizeBytes()));
            txt.setCursor(2, row++);
            txt.printf("Pixel format: %s",
                       pixelFormatName(c.framebuffer->pixelFormat()));
        }
        txt.setCursor(2, row++);
        txt.printf("Text grid   : %ld x %ld cells",
                   static_cast<long>(txt.columns()),
                   static_cast<long>(txt.rows()));
        txt.setCursor(2, row + 1);
        txt.print("BOOTSEL: next pattern");
    }
};

/// Validates: nothing on screen - it is an oscilloscope companion.
/// Expected: the printed compile-time timings match probe measurements
/// on HSYNC/VSYNC/VIDEO within crystal tolerance.
/// Defects indicate: wrong system clock, wrong divider snapping, or a
/// timing-table builder regression (measured != printed).
class TimingsPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Timings"; }

    void draw(DiagnosticContext& c) override {
        c.graphics.clear(c.palette.black);
        const VideoTiming& t = timingFor(c.mode);
        Text& txt = c.text;
        txt.setUnderline(false);
        txt.setColors(c.palette.bright, c.palette.black);
        txt.setCursor(2, 1);
        txt.print("Expected signal timings (compile-time, for scope probing)");
        txt.setColors(c.palette.normal, c.palette.black);

        std::int32_t row = 3;
        const auto span = [&](const char* label, std::uint32_t pixels) {
            const std::uint64_t us100 = static_cast<std::uint64_t>(pixels) *
                                        100000000ull / t.pixelClockHz;
            txt.setCursor(2, row++);
            txt.printf("%-22s %6lu px  %5lu.%02lu us", label,
                       static_cast<unsigned long>(pixels),
                       static_cast<unsigned long>(us100 / 100u),
                       static_cast<unsigned long>(us100 % 100u));
        };
        span("HSYNC period (total)", t.hTotal());
        span("  active", t.hActive);
        span("  front porch", t.hFrontPorch);
        span("  sync width", t.hSyncWidth);
        span("  back porch", t.hBackPorch);
        ++row;
        span("VSYNC period (total)", t.hTotal() * t.vTotal());
        const auto lines = [&](const char* label, std::uint32_t count) {
            txt.setCursor(2, row++);
            txt.printf("%-22s %6lu lines", label,
                       static_cast<unsigned long>(count));
        };
        lines("  active", t.vActive);
        lines("  front porch", t.vFrontPorch);
        lines("  sync width", t.vSyncWidth);
        lines("  back porch", t.vBackPorch);
    }
};

// ---------------------------------------------------------------------------
// Raster extent
// ---------------------------------------------------------------------------

/// Validates: full-field output at one level (white or black).
/// Expected: perfectly uniform field; with white, the lit area reaches
/// the raster limits; with black, no stray lit pixels anywhere.
/// Defects indicate: brightness/uniformity problems, HV regulation
/// issues (breathing), framebuffer initialization faults (black field
/// with lit specks), beam-current limiting (white field distortion).
class FullFieldPattern final : public DiagnosticPattern {
public:
    FullFieldPattern(const char* patternName, bool bright)
        : name_(patternName), bright_(bright) {}

    const char* name() const override { return name_; }

    void draw(DiagnosticContext& c) override {
        c.graphics.clear(bright_ ? c.palette.bright : c.palette.black);
    }

private:
    const char* name_;
    bool bright_;
};

/// Validates: the exact visible raster limits (all four outermost pixel
/// rows/columns) and centering.
/// Expected: an unbroken one-pixel bright frame with a centred normal
/// crosshair; the printed size matches the mode.
/// Defects indicate: off-by-one raster errors (a missing side),
/// overscan misadjustment (frame cut by the bezel), centering drift.
class BorderPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Raster border"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        g.clear(c.palette.black);
        g.drawHorizontalLine(0, 0, w, c.palette.bright);
        g.drawHorizontalLine(0, h - 1, w, c.palette.bright);
        g.drawVerticalLine(0, 0, h, c.palette.bright);
        g.drawVerticalLine(w - 1, 0, h, c.palette.bright);
        g.drawHorizontalLine(0, h / 2, w, c.palette.normal);
        g.drawVerticalLine(w / 2, 0, h, c.palette.normal);
        c.text.setColors(c.palette.bright, c.palette.black);
        c.text.setUnderline(false);
        c.text.setCursor(c.text.columns() / 2 - 5, c.text.rows() / 2 - 2);
        c.text.printf("%d x %d", w, h);
    }
};

/// Validates: usable raster extent on the PHYSICAL monitor, without
/// recompiling. The active timing never changes: the adjustable margins
/// only select among candidate visible geometries inside the raster the
/// mode already generates (run it in the overscan mode for the full
/// exploration range). The area outside the margin is dimmed, the
/// candidate area is framed bright.
/// Expected: pressing BOOTSEL steps through margin presets; the frame
/// that remains fully visible on the bezel is the monitor's usable
/// raster; note its printed size.
/// Defects indicate: nothing electrical - this is a measurement tool
/// for choosing per-monitor overscan constraints.
class AdjustableRasterPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Adjustable raster"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        const std::int32_t mh = kPresets[preset_].horizontal;
        const std::int32_t mv = kPresets[preset_].vertical;
        // Dim everything, then black out and frame the candidate area.
        g.clear(c.palette.dim);
        for (std::int32_t y = mv; y < h - mv; ++y) {
            g.drawHorizontalLine(mh, y, w - 2 * mh, c.palette.black);
        }
        g.drawHorizontalLine(mh, mv, w - 2 * mh, c.palette.bright);
        g.drawHorizontalLine(mh, h - 1 - mv, w - 2 * mh, c.palette.bright);
        g.drawVerticalLine(mh, mv, h - 2 * mv, c.palette.bright);
        g.drawVerticalLine(w - 1 - mh, mv, h - 2 * mv, c.palette.bright);

        Text& t = c.text;
        t.setUnderline(false);
        t.setColors(c.palette.normal, c.palette.black);
        t.setCursor(t.columns() / 2 - 14, t.rows() / 2 - 1);
        t.printf("H margin %2ld px  V margin %2ld lines",
                 static_cast<long>(mh), static_cast<long>(mv));
        t.setCursor(t.columns() / 2 - 14, t.rows() / 2);
        t.printf("candidate raster %ld x %ld",
                 static_cast<long>(w - 2 * mh), static_cast<long>(h - 2 * mv));
        t.setCursor(t.columns() / 2 - 14, t.rows() / 2 + 1);
        t.print("BOOTSEL: next margin preset");
    }

    bool onButtonPress(DiagnosticContext& c) override {
        ++preset_;
        if (preset_ < static_cast<std::int32_t>(sizeof kPresets /
                                                sizeof kPresets[0])) {
            draw(c);
            return true;
        }
        preset_ = 0;
        return false; // advance to the next pattern
    }

private:
    struct Margins {
        std::int32_t horizontal;
        std::int32_t vertical;
    };
    static constexpr Margins kPresets[] = {{0, 0},  {2, 1},  {4, 2}, {6, 3},
                                           {8, 4},  {12, 6}, {16, 8}};

    std::int32_t preset_ = 0;
};

// ---------------------------------------------------------------------------
// Geometry and convergence
// ---------------------------------------------------------------------------

/// Validates: horizontal/vertical linearity and (on color CRTs)
/// convergence; the classic crosshatch.
/// Expected: evenly spaced straight lines over the whole screen; outer
/// bright lines exactly at the raster edge.
/// Defects indicate: linearity errors (spacing varies), pincushion or
/// barrel distortion (lines bow), yoke tilt (rotated grid).
class GridPattern final : public DiagnosticPattern {
public:
    GridPattern(const char* patternName, std::int32_t cellSize)
        : name_(patternName), cellSize_(cellSize) {}

    const char* name() const override { return name_; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        g.clear(c.palette.black);
        for (std::int32_t x = 0; x < w; x += cellSize_) {
            g.drawVerticalLine(x, 0, h, c.palette.normal);
        }
        for (std::int32_t y = 0; y < h; y += cellSize_) {
            g.drawHorizontalLine(0, y, w, c.palette.normal);
        }
        g.drawVerticalLine(w - 1, 0, h, c.palette.bright);
        g.drawHorizontalLine(0, h - 1, w, c.palette.bright);
    }

private:
    const char* name_;
    std::int32_t cellSize_;
};

/// Validates: spot size, focus uniformity and convergence at isolated
/// points (harder than lines).
/// Expected: single bright dots on a regular grid, round and equally
/// sharp in the corners and the centre.
/// Defects indicate: focus falloff towards corners (blooming dots),
/// astigmatism (oval dots), poor convergence (colored fringes on color
/// monitors).
class DotGridPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Dot grid"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        g.clear(c.palette.black);
        for (std::int32_t y = 8; y < g.height(); y += 16) {
            for (std::int32_t x = 8; x < g.width(); x += 16) {
                g.drawPixel(x, y, c.palette.bright);
            }
        }
    }
};

/// Validates: centering and static convergence at the screen centre.
/// Expected: one full-screen cross through the exact centre with a
/// small centre box; arms reach the raster edges.
/// Defects indicate: centering-magnet misadjustment (cross off the
/// tube's physical centre), asymmetric deflection.
class CenterCrossPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Center cross"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        g.clear(c.palette.black);
        g.drawHorizontalLine(0, h / 2, w, c.palette.bright);
        g.drawVerticalLine(w / 2, 0, h, c.palette.bright);
        for (std::int32_t i = 0; i < 3; ++i) { // centre box 24x24
            const std::int32_t s = 12;
            g.drawHorizontalLine(w / 2 - s, h / 2 - s + i * s, 2 * s + 1,
                                 c.palette.normal);
            g.drawVerticalLine(w / 2 - s + i * s, h / 2 - s, 2 * s + 1,
                               c.palette.normal);
        }
    }
};

/// Validates: aspect ratio and corner geometry using squares of known
/// pixel size.
/// Expected: outlined squares at the centre and all four corners. NOTE:
/// pixels are not square on every mode (e.g. MDA); squares then appear
/// as rectangles of a CONSTANT aspect everywhere on screen.
/// Defects indicate: trapezoid/keystone errors (corner squares differ
/// from each other), width/height misadjustment (centre aspect varies
/// from the corners').
class GeometrySquaresPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Geometry squares"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        const std::int32_t side = h / 5;
        g.clear(c.palette.black);
        square(g, (w - side) / 2, (h - side) / 2, side, c.palette.bright);
        const std::int32_t inset = 8;
        square(g, inset, inset, side, c.palette.normal);
        square(g, w - inset - side, inset, side, c.palette.normal);
        square(g, inset, h - inset - side, side, c.palette.normal);
        square(g, w - inset - side, h - inset - side, side, c.palette.normal);
    }

private:
    static void square(Graphics& g, std::int32_t x, std::int32_t y,
                       std::int32_t side, Color color) {
        g.drawHorizontalLine(x, y, side, color);
        g.drawHorizontalLine(x, y + side - 1, side, color);
        g.drawVerticalLine(x, y, side, color);
        g.drawVerticalLine(x + side - 1, y, side, color);
    }
};

/// Validates: deflection linearity in every direction at once.
/// Expected: concentric pixel-perfect circles from the centre. NOTE: on
/// modes with non-square pixels they appear as ellipses of a CONSTANT
/// aspect; what matters is that the shape is smooth and identical in
/// every quadrant.
/// Defects indicate: non-linearity (egg-shaped rings), quadrant
/// asymmetry (deflection drive), ringing (ripple on vertical edges).
class CirclesPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Circles"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        g.clear(c.palette.black);
        const std::int32_t step = h / 8;
        if (step <= 0) {
            return; // Surface under 8 pixels tall: nothing to draw
        }           // (and step 0 must not stall the loop below).
        for (std::int32_t r = step; r <= h / 2; r += step) {
            circle(g, w / 2, h / 2, r,
                   r == (h / 2 / step) * step ? c.palette.bright
                                              : c.palette.normal);
        }
        g.drawPixel(w / 2, h / 2, c.palette.bright);
    }

private:
    static void circle(Graphics& g, std::int32_t cx, std::int32_t cy,
                       std::int32_t r, Color color) {
        std::int32_t x = r;
        std::int32_t y = 0;
        std::int32_t err = 1 - r;
        while (x >= y) {
            g.drawPixel(cx + x, cy + y, color);
            g.drawPixel(cx + y, cy + x, color);
            g.drawPixel(cx - x, cy + y, color);
            g.drawPixel(cx - y, cy + x, color);
            g.drawPixel(cx + x, cy - y, color);
            g.drawPixel(cx + y, cy - x, color);
            g.drawPixel(cx - x, cy - y, color);
            g.drawPixel(cx - y, cy - x, color);
            ++y;
            if (err < 0) {
                err += 2 * y + 1;
            } else {
                --x;
                err += 2 * (y - x) + 1;
            }
        }
    }
};

/// Validates: diagonal rendering and video-chain bandwidth via moire
/// sensitivity.
/// Expected: clean parallel 45-degree lines with constant spacing and
/// no stair-step irregularity; faint moire against the CRT mask is
/// normal, coarse patterning is not.
/// Defects indicate: pixel clock jitter (ragged diagonals), bandwidth
/// limiting (brightness varies along the line), scan-timing errors
/// (kinked diagonals).
class DiagonalsPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Diagonals"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        g.clear(c.palette.black);
        for (std::int32_t d = 0; d < w + h; d += 12) {
            g.drawLine(d, 0, d - h, h - 1, c.palette.normal);
        }
    }
};

// ---------------------------------------------------------------------------
// Pixel-level and bandwidth patterns
// ---------------------------------------------------------------------------

/// Validates: the complete pixel path at a given block size; period 1
/// is the definitive full-bandwidth acid test, period 2 halves the
/// frequency for comparison.
/// Expected: perfectly uniform texture (flat gray from a distance);
/// both periods stable and unshimmering.
/// Defects indicate: packing/bit-order regressions (vertical banding),
/// bandwidth limits (period 1 dimmer than period 2), clock instability
/// (swimming pattern).
class CheckerboardPattern final : public DiagnosticPattern {
public:
    CheckerboardPattern(const char* patternName, std::int32_t period)
        : name_(patternName), period_(period) {}

    const char* name() const override { return name_; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        g.clear(c.palette.black);
        for (std::int32_t y = 0; y < g.height(); ++y) {
            for (std::int32_t x = 0; x < g.width(); ++x) {
                if (((x / period_ + y / period_) & 1) == 0) {
                    g.drawPixel(x, y, c.palette.normal);
                }
            }
        }
    }

private:
    const char* name_;
    std::int32_t period_;
};

/// Validates: stripe rendering at a configurable period in either
/// orientation; period 1 vertical stripes are the maximum-frequency
/// video signal, period 1 horizontal stripes stress interlace-free
/// line addressing.
/// Expected: uniform stripes, stable, evenly bright across the screen.
/// Defects indicate: vertical stripes - bandwidth/clock problems;
/// horizontal stripes - line addressing or sync instability (pairing,
/// rolling).
class BarsPattern final : public DiagnosticPattern {
public:
    BarsPattern(const char* patternName, bool vertical, std::int32_t period)
        : name_(patternName), vertical_(vertical), period_(period) {}

    const char* name() const override { return name_; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        g.clear(c.palette.black);
        if (vertical_) {
            for (std::int32_t x = 0; x < g.width(); ++x) {
                if ((x / period_) & 1) {
                    g.drawVerticalLine(x, 0, g.height(), c.palette.bright);
                }
            }
        } else {
            for (std::int32_t y = 0; y < g.height(); ++y) {
                if ((y / period_) & 1) {
                    g.drawHorizontalLine(0, y, g.width(), c.palette.bright);
                }
            }
        }
    }

private:
    const char* name_;
    bool vertical_;
    std::int32_t period_;
};

// ---------------------------------------------------------------------------
// Levels
// ---------------------------------------------------------------------------

/// Validates: every electrical output level of the backend. On MDA:
/// black / VIDEO / INTENSITY-only / VIDEO+INTENSITY.
/// Expected: four labelled bands; BLACK fully dark, NORMAL and BRIGHT
/// clearly distinct, DIM as the monitor defines it (often invisible on
/// a 5151). Band edges razor sharp.
/// Defects indicate: a dead output line (two bands identical), signal
/// attenuation (weak level separation), edge skew between output lines
/// (colored/bright fringes at band boundaries).
class IntensityPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Intensity"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const std::int32_t w = g.width();
        const std::int32_t h = g.height();
        const Color bands[4] = {c.palette.black, c.palette.normal,
                                c.palette.dim, c.palette.bright};
        const char* labels[4] = {"BLACK", "NORMAL", "DIM", "BRIGHT"};
        g.clear(c.palette.black);
        const std::int32_t cellHeight =
            c.text.rows() > 0 ? h / c.text.rows() : 0;
        const std::int32_t top = 2 * cellHeight;
        for (int band = 0; band < 4; ++band) {
            const std::int32_t x0 = band * w / 4;
            const std::int32_t x1 = (band + 1) * w / 4;
            for (std::int32_t x = x0; x < x1; ++x) {
                g.drawVerticalLine(x, top, h - top, bands[band]);
            }
        }
        c.text.setColors(c.palette.bright, c.palette.black);
        c.text.setUnderline(false);
        const std::int32_t columnsPerBand = c.text.columns() / 4;
        for (int band = 0; band < 4; ++band) {
            c.text.setCursor(band * columnsPerBand + 1, 0);
            c.text.print(labels[band]);
        }
    }
};

/// Validates: level ordering and monotonicity across the palette - the
/// grayscale staircase.
/// Expected: left-to-right steps of increasing brightness with sharp
/// transitions (levels available depend on the backend's palette).
/// Defects indicate: swapped output bits (non-monotonic staircase),
/// level compression (adjacent steps identical).
class StaircasePattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Gray staircase"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        const Color ramp[4] = {c.palette.black, c.palette.dim,
                               c.palette.normal, c.palette.bright};
        const std::int32_t w = g.width();
        for (std::int32_t x = 0; x < w; ++x) {
            g.drawVerticalLine(x, 0, g.height(), ramp[(x * 4) / w]);
        }
    }
};

// ---------------------------------------------------------------------------
// Content patterns
// ---------------------------------------------------------------------------

/// Validates: glyph rendering quality and focus with real content,
/// including attributes.
/// Expected: rows cycling normal / bright / underline / inverse, all
/// character cells crisp everywhere on screen.
/// Defects indicate: focus problems (soft corners), attribute
/// regressions (wrong row styles), font-data corruption.
class TextQualityPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Text quality"; }

    void draw(DiagnosticContext& c) override {
        c.graphics.clear(c.palette.black);
        Text& t = c.text;
        for (std::int32_t row = 0; row < t.rows(); ++row) {
            t.setCursor(0, row);
            switch (row % 4) {
                case 0:
                    t.setColors(c.palette.normal, c.palette.black);
                    t.setUnderline(false);
                    break;
                case 1:
                    t.setColors(c.palette.bright, c.palette.black);
                    t.setUnderline(false);
                    break;
                case 2:
                    t.setColors(c.palette.normal, c.palette.black);
                    t.setUnderline(true);
                    break;
                default:
                    t.setColors(c.palette.black, c.palette.normal);
                    t.setUnderline(false);
                    break;
            }
            for (std::int32_t column = 0; column < t.columns(); ++column) {
                t.putChar(static_cast<char>(' ' + (column + row) % 95));
            }
        }
        t.setUnderline(false);
    }
};

/// Validates: raster stability while the framebuffer is continuously
/// rewritten (bus contention between CPU writes and scan-out DMA).
/// Expected: a bright square bouncing smoothly inside a rock-solid
/// static frame; minor shear on the moving square is acceptable (no
/// vsync-locked updates yet), frame wobble is not.
/// Defects indicate: memory-contention artifacts (frame jitter while
/// the square moves), DMA starvation (full-screen disturbance).
class AnimationPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Animation"; }

    void draw(DiagnosticContext& c) override {
        Graphics& g = c.graphics;
        g.clear(c.palette.black);
        g.drawHorizontalLine(0, 0, g.width(), c.palette.normal);
        g.drawHorizontalLine(0, g.height() - 1, g.width(), c.palette.normal);
        g.drawVerticalLine(0, 0, g.height(), c.palette.normal);
        g.drawVerticalLine(g.width() - 1, 0, g.height(), c.palette.normal);
        lastX_ = -1;
    }

    void update(DiagnosticContext& c, std::uint32_t nowMs) override {
        Graphics& g = c.graphics;
        const std::int32_t range = g.width() - kSize - 4;
        if (range <= 0) {
            return;
        }
        const std::int32_t phase =
            static_cast<std::int32_t>((nowMs / 4) % (2 * range));
        const std::int32_t x = 2 + (phase < range ? phase : 2 * range - phase);
        const std::int32_t y = (g.height() - kSize) / 2;
        if (x == lastX_) {
            return;
        }
        if (lastX_ >= 0) {
            fillSquare(g, lastX_, y, c.palette.black);
        }
        fillSquare(g, x, y, c.palette.bright);
        lastX_ = x;
    }

private:
    static constexpr std::int32_t kSize = 24;

    static void fillSquare(Graphics& g, std::int32_t x, std::int32_t y,
                           Color color) {
        for (std::int32_t i = 0; i < kSize; ++i) {
            g.drawHorizontalLine(x, y + i, kSize, color);
        }
    }

    std::int32_t lastX_ = -1;
};

} // namespace picottl::diagnostics
