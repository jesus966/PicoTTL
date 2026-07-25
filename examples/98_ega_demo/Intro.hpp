// PicoTTL - Demonstration 98: IBM Enhanced Graphics Adapter (intro).
//
// Animated opening sequence: the sixty-four color gamut sweeps onto
// the screen, then the PicoTTL banner is built from the EGA's own
// 8x14 character generator, framed and subtitled.
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
#include "picottl/fonts/IbmEga8x14.hpp"

namespace ega_demo {

namespace ega = picottl::ega;

inline const picottl::demo::ScreenColors kScreen = {
    ega::kBlack,     // background
    ega::kLightBlue, // frame
    ega::kWhite,     // heading
    ega::kLightGray, // label
    ega::kWhite,     // value
};

inline void runIntro(picottl::Graphics& g, picottl::Text& text) {
    namespace demo = picottl::demo;

    // 1. All sixty-four color values sweep in, bar by bar - the
    //    statement CGA could never make.
    g.clear(ega::kBlack);
    sleep_ms(600);
    for (int i = 0; i < 64; ++i) {
        demo::fillRect(g, i * 10, 0, 10, 350, picottl::Color{
            static_cast<std::uint8_t>(i)});
        sleep_ms(18);
    }
    sleep_ms(900);
    demo::wipeDown(g, ega::kBlack, 14, 8);

    // 2. Banner: "PicoTTL" from the EGA 8x14 generator, magnified 4x2,
    //    with a deep blue drop shadow. Letters appear one at a time.
    const auto& font = picottl::fonts::kIbmEga8x14;
    const char* kTitle = "PicoTTL";
    const std::int32_t scaleX = 4;
    const std::int32_t scaleY = 2;
    const std::int32_t bannerWidth = demo::bigTextWidth(font, kTitle, scaleX);
    const std::int32_t bannerX = (g.width() - bannerWidth) / 2;
    const std::int32_t bannerY = 88;
    {
        char letter[2] = {0, 0};
        std::int32_t penX = bannerX;
        for (const char* p = kTitle; *p != '\0'; ++p) {
            letter[0] = *p;
            demo::drawBigText(g, font, letter, penX + 3, bannerY + 2,
                              scaleX, scaleY, picottl::Color{0x08});
            demo::drawBigText(g, font, letter, penX, bannerY,
                              scaleX, scaleY, ega::kWhite);
            penX += font.glyphWidth * scaleX;
            sleep_ms(160);
        }
    }
    // A sixty-four color rule sweeps in beneath the banner.
    const std::int32_t ruleX = (g.width() - 64 * 8) / 2;
    for (int i = 0; i < 64; ++i) {
        demo::fillRect(g, ruleX + i * 8,
                       bannerY + font.glyphHeight * scaleY + 8, 8, 4,
                       picottl::Color{static_cast<std::uint8_t>(i)});
        sleep_ms(12);
    }

    // 3. Typed subtitle and frame.
    text.setColors(ega::kLightGray, ega::kBlack);
    demo::typeCentered(text, 12, "IBM Enhanced Graphics Adapter", 22);
    demo::typeCentered(text, 14, "D e m o n s t r a t i o n", 22);
    sleep_ms(400);
    text.setColors(ega::kLightBlue, ega::kBlack);
    demo::drawBox(text, 14, 3, 52, 14, demo::BoxStyle::Double);
    text.setColors(ega::kLightGray, ega::kBlack);
    demo::printCentered(text, 19, "PicoTTL drives this monitor from a");
    demo::printCentered(text, 20, "Raspberry Pi Pico 2 - no PC required");
    sleep_ms(3200);

    demo::wipeDown(g, ega::kBlack, 14, 8);
}

} // namespace ega_demo
