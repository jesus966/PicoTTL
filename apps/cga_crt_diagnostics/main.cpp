// PicoTTL CGA CRT Diagnostics
//
// OFFICIAL PicoTTL APPLICATION - the permanent validation suite for
// the CGA backend and for RGBI monitors (IBM 5153 and compatibles),
// sibling of the MDA CRT diagnostics. Both applications share the
// backend-agnostic pattern framework and the generic pattern catalog
// (apps/diagnostics_common) but evolve independently: this one adds
// the color diagnostics an RGBI monitor family needs.
//
// It exercises exactly the same public API normal applications use -
// including dynamic 640<->320 mode switching - and never relies on
// concrete mode geometry: all sizes are queried through the Display
// API.
//
// Controls: a SHORT BOOTSEL press advances to the next pattern (or
// moves the selection inside "Mode select"); a LONG press (~1.5 s)
// activates the selection in "Mode select" and skips ahead elsewhere.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// monitor signal wires directly):
//   GPIO 2  -> HSYNC     (DE-9 pin 8)
//   GPIO 3  -> VSYNC     (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 16 -> BLUE      (DE-9 pin 5)
//   GPIO 17 -> GREEN     (DE-9 pin 4)
//   GPIO 18 -> RED       (DE-9 pin 3)
//   GPIO 19 -> INTENSITY (DE-9 pin 6)
//   GND     -> GND       (DE-9 pin 1 only)
//
// NOTE: this is the AUTHENTIC STANDALONE CGA ADAPTER pinout, not the
// PicoTTL reference connector (EGA superset, where DE-9 pin 6 is fed
// from GPIO 20). On the reference connector, diagnose CGA monitors
// with the EGA diagnostics app in its 200-line modes instead (see
// docs/architecture.md).
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/fonts/IbmCga8x8.hpp"
#include "picottl/rp2350/CgaDisplayDevice.hpp"

#include "BootselButton.hpp"
#include "CgaPatterns.hpp"
#include "DiagnosticPattern.hpp"
#include "Patterns.hpp"

namespace {

using namespace picottl;
using namespace picottl::diagnostics;

// ---------------------------------------------------------------------------
// Framebuffers: one storage block sized for the largest supported mode,
// wrapped by one framebuffer object per mode geometry.
// ---------------------------------------------------------------------------

constexpr VideoMode kModes[] = {VideoMode::IbmCga640, VideoMode::IbmCga320};
constexpr std::size_t kModeCount = sizeof kModes / sizeof kModes[0];

constexpr std::size_t kStorageBytes = PackedFramebuffer4::bytesFor(
    modeWidth(VideoMode::IbmCga640), modeHeight(VideoMode::IbmCga640));
static_assert(kStorageBytes >= PackedFramebuffer4::bytesFor(
                                   modeWidth(VideoMode::IbmCga320),
                                   modeHeight(VideoMode::IbmCga320)),
              "shared storage must cover every supported mode");

alignas(std::uint32_t) std::uint8_t gStorage[kStorageBytes];

PackedFramebuffer4 gFramebuffers[kModeCount] = {
    {gStorage, sizeof gStorage, modeWidth(kModes[0]), modeHeight(kModes[0])},
    {gStorage, sizeof gStorage, modeWidth(kModes[1]), modeHeight(kModes[1])},
};

Framebuffer* framebufferFor(VideoMode mode) {
    for (std::size_t i = 0; i < kModeCount; ++i) {
        if (kModes[i] == mode) {
            return &gFramebuffers[i];
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Mode switching through the PUBLIC API only.
// ---------------------------------------------------------------------------

bool gModeSwitchFailed = false;

bool applyMode(Display& display, VideoMode mode) {
    const VideoMode previous = display.mode();
    display.end();
    Framebuffer* framebuffer = framebufferFor(mode);
    if (framebuffer != nullptr && display.setFramebuffer(framebuffer) &&
        display.begin(mode)) {
        gModeSwitchFailed = false;
        return true;
    }
    // Never leave the monitor without a raster: restore the previous
    // mode and report the failure on screen.
    gModeSwitchFailed = true;
    display.setFramebuffer(framebufferFor(previous));
    display.begin(previous);
    return false;
}

/// Mode selection pattern: each press applies the next supported CGA
/// raster (immediate feedback on the CRT); after "CONTINUE" the press
/// advances to the next pattern like everywhere else.
class ModeSelectPattern final : public DiagnosticPattern {
public:
    const char* name() const override { return "Mode select"; }

    void draw(DiagnosticContext& c) override {
        c.graphics.clear(c.palette.black);
        Text& t = c.text;
        t.setUnderline(false);
        t.setColors(c.palette.bright, c.palette.black);
        t.setCursor(2, 1);
        t.print("Mode select");
        t.setColors(c.palette.normal, c.palette.black);
        t.setCursor(2, 2);
        t.print("BOOTSEL: press = move selection, HOLD = apply");
        for (std::size_t i = 0; i < kModeCount; ++i) {
            const bool isActive = kModes[i] == c.mode;
            const bool isSelected = selection_ == static_cast<std::int32_t>(i);
            t.setColors(isActive ? c.palette.bright : c.palette.normal,
                        c.palette.black);
            t.setCursor(2, 4 + static_cast<std::int32_t>(i));
            t.printf("%c %s %dx%d (%s)", isSelected ? '>' : ' ',
                     isActive ? "*" : " ", modeWidth(kModes[i]),
                     modeHeight(kModes[i]), modeName(kModes[i]));
        }
        t.setColors(c.palette.normal, c.palette.black);
        t.setCursor(2, 5 + static_cast<std::int32_t>(kModeCount));
        t.printf("%c   CONTINUE",
                 selection_ == static_cast<std::int32_t>(kModeCount) ? '>' : ' ');
        if (gModeSwitchFailed) {
            t.setColors(c.palette.bright, c.palette.black);
            t.setCursor(2, 7 + static_cast<std::int32_t>(kModeCount));
            t.print("! LAST MODE SWITCH FAILED - PREVIOUS MODE RESTORED");
        }
    }

    bool onButtonPress(DiagnosticContext& c) override {
        // Short press: move the selection only - never applies a mode,
        // never advances the pattern.
        selection_ =
            (selection_ + 1) % static_cast<std::int32_t>(kModeCount + 1);
        draw(c);
        return true;
    }

    bool onButtonLongPress(DiagnosticContext& c) override {
        // Long press: activate the selection.
        if (selection_ < static_cast<std::int32_t>(kModeCount)) {
            applyMode(c.display, kModes[selection_]);
            return true; // the app rebuilds the context and redraws us
        }
        selection_ = 0;
        return false; // CONTINUE: advance to the next pattern
    }

private:
    std::int32_t selection_ = 0;
};

// ---------------------------------------------------------------------------
// Pattern registry: adding a diagnostic = one class + one entry here.
// ---------------------------------------------------------------------------

// Information + navigation.
InfoPattern gInfo;
TimingsPattern gTimings;
ModeSelectPattern gModeSelect;
AdjustableRasterPattern gAdjustableRaster; // safe-area exploration

// Geometry (generic catalog).
FullFieldPattern gFullWhite{"Full white", true};
FullFieldPattern gFullBlack{"Full black", false};
BorderPattern gBorder;
CenterCrossPattern gCenterCross;
GridPattern gCoarseGrid{"Crosshatch 32", 32};
GridPattern gFineGrid{"Crosshatch 8", 8};
DotGridPattern gDotGrid;
GeometrySquaresPattern gGeometrySquares;
CirclesPattern gCircles;
DiagonalsPattern gDiagonals;
CheckerboardPattern gChecker1{"Checkerboard 1x1", 1};
CheckerboardPattern gChecker2{"Checkerboard 2x2", 2};
BarsPattern gFineVStripes{"Vertical stripes 1px", true, 1};
BarsPattern gFineHStripes{"Horizontal stripes 1px", false, 1};

// Color (CGA-specific).
CgaSolidColorsPattern gSolidColors;
CgaColorBarsPattern gVColorBars{"Color bars vertical", true};
CgaColorBarsPattern gHColorBars{"Color bars horizontal", false};
CgaRgbiBitsPattern gRgbiBits;
CgaPrimariesPattern gPrimaries;
CgaIntensityRampPattern gIntensityRamp;
CgaTransitionsPattern gTransitions;

// Text and terminal.
TextQualityPattern gTextQuality;
CgaCharacterSetPattern gCharacterSet;
CgaAnsiColorsPattern gAnsiColors;
CgaTerminalPattern gTerminal;

// Animation.
AnimationPattern gAnimation;
CgaColorCyclePattern gColorCycle;

DiagnosticPattern* const gPatterns[] = {
    &gInfo,         &gTimings,       &gModeSelect,     &gAdjustableRaster,
    &gFullWhite,    &gFullBlack,     &gBorder,         &gCenterCross,
    &gCoarseGrid,   &gFineGrid,      &gDotGrid,        &gGeometrySquares,
    &gCircles,      &gDiagonals,     &gChecker1,       &gChecker2,
    &gFineVStripes, &gFineHStripes,  &gSolidColors,    &gVColorBars,
    &gHColorBars,   &gRgbiBits,      &gPrimaries,      &gIntensityRamp,
    &gTransitions,  &gTextQuality,   &gCharacterSet,   &gAnsiColors,
    &gTerminal,     &gAnimation,     &gColorCycle,
};
constexpr std::size_t kPatternCount = sizeof gPatterns / sizeof gPatterns[0];

} // namespace

int main() {
    // 142.8 MHz: jitter-free integer dividers for both CGA modes,
    // vertical ~59.76 Hz (slightly below nominal so aged monitors'
    // vertical oscillators capture - hardware-validated choice).
    set_sys_clock_khz(142'800, true);

    rp2350::CgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3; // GPIO 4 internal handshake, leave unwired.
    config.bluePin = 16;
    config.greenPin = 17;
    config.redPin = 18;
    config.intensityPin = 19;
    config.framebuffer = framebufferFor(VideoMode::IbmCga640);

    rp2350::CgaDisplayDevice device(config);
    Display display(device);
    Text text(display.graphics(), fonts::kIbmCga8x8);

    const bool ok = display.begin(VideoMode::IbmCga640);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    // The context is rebuilt whenever the mode (and thus surface,
    // geometry or format) may have changed.
    const auto makeContext = [&]() -> DiagnosticContext {
        Framebuffer* framebuffer = framebufferFor(display.mode());
        return DiagnosticContext{
            display,
            display.graphics(),
            text,
            paletteFor(framebuffer != nullptr ? framebuffer->pixelFormat()
                                              : PixelFormat::Packed4),
            display.mode(),
            framebuffer,
            "RP2350 CGA family (PIO + DMA)"};
    };

    std::size_t patternIndex = 0;
    VideoMode drawnMode = display.mode();
    {
        DiagnosticContext context = makeContext();
        gPatterns[patternIndex]->draw(context);
    }

    bool lastButton = false;
    std::uint32_t lastEdgeMs = 0;
    std::uint32_t pressStartMs = 0;
    bool longPressFired = false;
    constexpr std::uint32_t kLongPressMs = 1500;
    const auto dispatch = [&](bool longPress) {
        DiagnosticContext context = makeContext();
        const bool consumed =
            longPress ? gPatterns[patternIndex]->onButtonLongPress(context)
                      : gPatterns[patternIndex]->onButtonPress(context);
        if (!consumed) {
            patternIndex = (patternIndex + 1) % kPatternCount;
        }
        // The press may have switched modes: rebuild and redraw.
        DiagnosticContext fresh = makeContext();
        gPatterns[patternIndex]->draw(fresh);
        drawnMode = display.mode();
    };
    while (true) {
        const std::uint32_t nowMs = to_ms_since_boot(get_absolute_time());

        const bool button = bootselButtonPressed();
        if (button != lastButton && nowMs - lastEdgeMs > 30) { // debounce
            lastButton = button;
            lastEdgeMs = nowMs;
            if (button) {
                pressStartMs = nowMs;
                longPressFired = false;
            } else if (!longPressFired) {
                dispatch(false); // short press: released before the hold
            }
        }
        if (lastButton && !longPressFired &&
            nowMs - pressStartMs >= kLongPressMs) {
            longPressFired = true; // fire once while still held
            dispatch(true);
        }

        if (display.mode() != drawnMode) { // safety: external mode change
            DiagnosticContext context = makeContext();
            gPatterns[patternIndex]->draw(context);
            drawnMode = display.mode();
        }

        {
            DiagnosticContext context = makeContext();
            gPatterns[patternIndex]->update(context, nowMs);
        }
        sleep_ms(5);
    }
}
