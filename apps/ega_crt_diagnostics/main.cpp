// PicoTTL EGA CRT Diagnostics
//
// OFFICIAL PicoTTL APPLICATION - the permanent validation suite for
// the EGA backend and for EGA monitors (IBM 5154 Enhanced Color
// Display and compatibles), sibling of the MDA and CGA CRT
// diagnostics. All three share the backend-agnostic pattern framework
// and the generic pattern catalog (apps/diagnostics_common) but evolve
// independently: this one adds the 64-color sections a six-wire rgbRGB
// monitor family needs.
//
// It exercises exactly the same public API normal applications use -
// including dynamic mode switching ACROSS CRYSTAL DOMAINS: the
// 350-line raster and the CGA-compatible 200-line rasters need
// different system clocks, so the "Mode select" pattern also
// demonstrates application-side reclocking.
//
// Controls: a SHORT BOOTSEL press advances to the next pattern (or
// moves the selection inside "Mode select"); a LONG press (~1.5 s)
// activates the selection in "Mode select" and skips ahead elsewhere.
//
// Wiring (470 ohm series resistors on HSYNC and VSYNC; connect all other
// monitor signal wires directly):
//   GPIO 2  -> HSYNC           (DE-9 pin 8)
//   GPIO 3  -> VSYNC           (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 16 -> PRIMARY BLUE    (DE-9 pin 5)
//   GPIO 17 -> PRIMARY GREEN   (DE-9 pin 4)
//   GPIO 18 -> PRIMARY RED     (DE-9 pin 3)
//   GPIO 19 -> SECONDARY BLUE  (DE-9 pin 7)
//   GPIO 20 -> SECONDARY GREEN (DE-9 pin 6)
//   GPIO 21 -> SECONDARY RED   (DE-9 pin 2)
//   GND     -> GND             (DE-9 pin 1 - pin 2 is Secondary Red,
//                               NOT a ground, unlike CGA)
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/fonts/IbmEga8x14.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"

#include "BootselButton.hpp"
#include "DiagnosticPattern.hpp"
#include "EgaPatterns.hpp"
#include "Patterns.hpp"

namespace {

using namespace picottl;
using namespace picottl::diagnostics;

// ---------------------------------------------------------------------------
// Framebuffers: one storage block sized for the largest supported mode,
// wrapped by one framebuffer object per mode geometry.
// ---------------------------------------------------------------------------

constexpr VideoMode kModes[] = {VideoMode::IbmEga350, VideoMode::IbmCga640,
                                VideoMode::IbmCga320};
/// Per-mode system clock: the two raster families live in different
/// crystal domains (see EgaDisplayDevice.hpp).
constexpr std::uint32_t kClockKhz[] = {130'000, 142'800, 142'800};
constexpr std::size_t kModeCount = sizeof kModes / sizeof kModes[0];

constexpr std::size_t kStorageBytes = PackedFramebuffer8::bytesFor(
    modeWidth(VideoMode::IbmEga350), modeHeight(VideoMode::IbmEga350));
static_assert(kStorageBytes >= PackedFramebuffer8::bytesFor(
                                   modeWidth(VideoMode::IbmCga640),
                                   modeHeight(VideoMode::IbmCga640)),
              "shared storage must cover every supported mode");

alignas(std::uint32_t) std::uint8_t gStorage[kStorageBytes];

PackedFramebuffer8 gFramebuffers[kModeCount] = {
    {gStorage, sizeof gStorage, modeWidth(kModes[0]), modeHeight(kModes[0])},
    {gStorage, sizeof gStorage, modeWidth(kModes[1]), modeHeight(kModes[1])},
    {gStorage, sizeof gStorage, modeWidth(kModes[2]), modeHeight(kModes[2])},
};

Framebuffer* framebufferFor(VideoMode mode) {
    for (std::size_t i = 0; i < kModeCount; ++i) {
        if (kModes[i] == mode) {
            return &gFramebuffers[i];
        }
    }
    return nullptr;
}

std::uint32_t clockFor(VideoMode mode) {
    for (std::size_t i = 0; i < kModeCount; ++i) {
        if (kModes[i] == mode) {
            return kClockKhz[i];
        }
    }
    return kClockKhz[0];
}

/// Backend-appropriate diagnostic palette, supplied by THIS app (which
/// knows the EGA backend's documented color contract) instead of the
/// format-derived shared heuristic: neutral grays in the 350-line
/// mode; in 200-line modes a standard 5154 reads the connector as CGA
/// (intensity = Secondary Green = bit 4), so the levels differ.
DiagnosticPalette egaPaletteFor(VideoMode mode) {
    if (mode == VideoMode::IbmEga350) {
        return {Color{0x00}, Color{0x38}, Color{0x07}, Color{0x3F}};
    }
    return {Color{0x00}, Color{0x10}, Color{0x07}, Color{0x17}};
}

// ---------------------------------------------------------------------------
// Mode switching through the PUBLIC API only - including the
// application-side reclock between crystal domains.
// ---------------------------------------------------------------------------

bool gModeSwitchFailed = false;

bool applyMode(Display& display, VideoMode mode) {
    const VideoMode previous = display.mode();
    display.end();
    set_sys_clock_khz(clockFor(mode), true);
    Framebuffer* framebuffer = framebufferFor(mode);
    if (framebuffer != nullptr && display.setFramebuffer(framebuffer) &&
        display.begin(mode)) {
        gModeSwitchFailed = false;
        return true;
    }
    // Never leave the monitor without a raster: restore the previous
    // mode (and its clock) and report the failure on screen.
    gModeSwitchFailed = true;
    set_sys_clock_khz(clockFor(previous), true);
    display.setFramebuffer(framebufferFor(previous));
    display.begin(previous);
    return false;
}

/// Mode selection pattern: each press applies the next supported
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
            t.printf("%c %s %dx%d (%s, %lu.%lu MHz)", isSelected ? '>' : ' ',
                     isActive ? "*" : " ", modeWidth(kModes[i]),
                     modeHeight(kModes[i]), modeName(kModes[i]),
                     static_cast<unsigned long>(kClockKhz[i] / 1000u),
                     static_cast<unsigned long>(kClockKhz[i] % 1000u / 100u));
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

// Color (EGA-specific).
Ega64ColorsPattern g64Colors;
EgaBitPlanesPattern gBitPlanes;
EgaGunLevelsPattern gGunLevels;
EgaGrayRampPattern gGrayRamp;
EgaDefaultPalettePattern gDefaultPalette;
EgaTransitionsPattern gTransitions;

// Text and terminal.
TextQualityPattern gTextQuality;
EgaCharacterSetPattern gCharacterSet;
EgaTerminalPattern gTerminal;

// Animation.
AnimationPattern gAnimation;
EgaColorCyclePattern gColorCycle;

DiagnosticPattern* const gPatterns[] = {
    &gInfo,          &gTimings,       &gModeSelect,   &gAdjustableRaster,
    &gFullWhite,     &gFullBlack,     &gBorder,       &gCenterCross,
    &gCoarseGrid,    &gFineGrid,      &gDotGrid,      &gGeometrySquares,
    &gCircles,       &gDiagonals,     &gChecker1,     &gChecker2,
    &gFineVStripes,  &gFineHStripes,  &g64Colors,     &gBitPlanes,
    &gGunLevels,     &gGrayRamp,      &gDefaultPalette, &gTransitions,
    &gTextQuality,   &gCharacterSet,  &gTerminal,     &gAnimation,
    &gColorCycle,
};
constexpr std::size_t kPatternCount = sizeof gPatterns / sizeof gPatterns[0];

} // namespace

int main() {
    set_sys_clock_khz(kClockKhz[0], true);

    rp2350::EgaDisplayDevice::Config config;
    config.hsyncPin = 2;
    config.vsyncPin = 3; // GPIO 4 internal handshake, leave unwired.
    config.primaryBluePin = 16;
    config.primaryGreenPin = 17;
    config.primaryRedPin = 18;
    config.secondaryBluePin = 19;
    config.secondaryGreenPin = 20;
    config.secondaryRedPin = 21;
    config.framebuffer = framebufferFor(VideoMode::IbmEga350);

    rp2350::EgaDisplayDevice device(config);
    Display display(device);
    Text text(display.graphics(), fonts::kIbmEga8x14);

    const bool ok = display.begin(VideoMode::IbmEga350);

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    // The context is rebuilt whenever the mode (and thus surface,
    // geometry or format) may have changed. The palette comes from
    // this app's backend knowledge, not the shared format heuristic.
    const auto makeContext = [&]() -> DiagnosticContext {
        return DiagnosticContext{
            display,
            display.graphics(),
            text,
            egaPaletteFor(display.mode()),
            display.mode(),
            framebufferFor(display.mode()),
            "RP2350 EGA family (PIO + DMA)"};
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
