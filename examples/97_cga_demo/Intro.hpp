// PicoTTL - Demonstration 97: IBM Color Graphics Adapter (intro).
//
// Animated opening sequence: the sixteen RGBI colors sweep onto the
// screen, then the PicoTTL banner is built from the CGA's own 8x8
// character generator with a drop shadow, framed and subtitled.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "pico/stdlib.h"

#include "DemoDraw.hpp"
#include "DemoText.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/Text.hpp"
#include "picottl/ega/Colors.hpp"
#include "picottl/fonts/IbmCga8x8.hpp"

namespace cga_demo {

// This demonstration runs on EgaDisplayDevice's CGA-compatible mode
// over the PicoTTL reference connector (docs/architecture.md, section
// 15) - exactly how IBM's EGA card drove CGA monitors. Every CGA
// color number i is therefore expressed as its EGA 200-line pixel
// value ega::kCga200Palette[i] (intensity on Secondary Green, DE-9
// pin 6). This local namespace keeps the demonstration's authentic
// CGA vocabulary; the values are the wire encoding.
namespace cga {
inline constexpr auto& kRgbi = picottl::ega::kCga200Palette;
inline constexpr picottl::Color kBlack = kRgbi[0];
inline constexpr picottl::Color kBlue = kRgbi[1];
inline constexpr picottl::Color kGreen = kRgbi[2];
inline constexpr picottl::Color kCyan = kRgbi[3];
inline constexpr picottl::Color kRed = kRgbi[4];
inline constexpr picottl::Color kMagenta = kRgbi[5];
inline constexpr picottl::Color kBrown = kRgbi[6];
inline constexpr picottl::Color kLightGray = kRgbi[7];
inline constexpr picottl::Color kDarkGray = kRgbi[8];
inline constexpr picottl::Color kLightBlue = kRgbi[9];
inline constexpr picottl::Color kLightGreen = kRgbi[10];
inline constexpr picottl::Color kLightCyan = kRgbi[11];
inline constexpr picottl::Color kLightRed = kRgbi[12];
inline constexpr picottl::Color kLightMagenta = kRgbi[13];
inline constexpr picottl::Color kYellow = kRgbi[14];
inline constexpr picottl::Color kWhite = kRgbi[15];
// The historical IBM 320x200 palettes (entry 0 = background).
inline constexpr picottl::Color kPalette0[4] = {kBlack, kGreen, kRed, kBrown};
inline constexpr picottl::Color kPalette0High[4] = {kBlack, kLightGreen,
                                                    kLightRed, kYellow};
inline constexpr picottl::Color kPalette1[4] = {kBlack, kCyan, kMagenta,
                                                kLightGray};
inline constexpr picottl::Color kPalette1High[4] = {kBlack, kLightCyan,
                                                    kLightMagenta, kWhite};
} // namespace cga

inline const picottl::demo::ScreenColors kScreen = {
    cga::kBlack,     // background
    cga::kLightCyan, // frame
    cga::kWhite,     // heading
    cga::kLightGray, // label
    cga::kWhite,     // value
};

inline void runIntro(picottl::Graphics& g, picottl::Text& text) {
    namespace demo = picottl::demo;

    // 1. The sixteen RGBI colors sweep in, bar by bar.
    g.clear(cga::kBlack);
    sleep_ms(600);
    for (int i = 0; i < 16; ++i) {
        demo::fillRect(g, i * 40, 0, 40, 200, cga::kRgbi[i]);
        sleep_ms(45);
    }
    sleep_ms(900);
    demo::wipeDown(g, cga::kBlack, 8, 8);

    // 2. Banner: "PicoTTL" from the CGA 8x8 generator, magnified 4x2
    //    (the tall CGA pixels do the vertical stretching), with the
    //    classic drop shadow. Letters appear one at a time.
    const auto& font = picottl::fonts::kIbmCga8x8;
    const char* kTitle = "PicoTTL";
    const std::int32_t scaleX = 4;
    const std::int32_t scaleY = 2;
    const std::int32_t bannerWidth = demo::bigTextWidth(font, kTitle, scaleX);
    const std::int32_t bannerX = (g.width() - bannerWidth) / 2;
    const std::int32_t bannerY = 52;
    {
        char letter[2] = {0, 0};
        std::int32_t penX = bannerX;
        for (const char* p = kTitle; *p != '\0'; ++p) {
            letter[0] = *p;
            demo::drawBigText(g, font, letter, penX + 3, bannerY + 1,
                              scaleX, scaleY, cga::kDarkGray);
            demo::drawBigText(g, font, letter, penX, bannerY,
                              scaleX, scaleY, cga::kWhite);
            penX += font.glyphWidth * scaleX;
            sleep_ms(160);
        }
    }
    // A sixteen-color rule sweeps in beneath the banner.
    for (int i = 0; i < 16; ++i) {
        demo::fillRect(g, bannerX + i * (bannerWidth / 16),
                       bannerY + font.glyphHeight * scaleY + 6,
                       bannerWidth / 16, 3, cga::kRgbi[i]);
        sleep_ms(30);
    }

    // 3. Typed subtitle and frame.
    text.setColors(cga::kLightGray, cga::kBlack);
    demo::typeCentered(text, 12, "IBM Color Graphics Adapter", 22);
    demo::typeCentered(text, 14, "D e m o n s t r a t i o n", 22);
    sleep_ms(400);
    text.setColors(cga::kLightCyan, cga::kBlack);
    demo::drawBox(text, 14, 3, 52, 14, demo::BoxStyle::Double);
    text.setColors(cga::kLightGray, cga::kBlack);
    demo::printCentered(text, 19, "PicoTTL drives this monitor from a");
    demo::printCentered(text, 20, "Raspberry Pi Pico 2 - no PC required");
    sleep_ms(3200);

    demo::wipeDown(g, cga::kBlack, 8, 10);
}

} // namespace cga_demo
