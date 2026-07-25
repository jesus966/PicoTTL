// PicoTTL - Demonstration 96: IBM Monochrome Display Adapter (intro).
//
// Animated opening sequence in the style of 1980s IBM software: a
// blinking cursor on a dark screen, BIOS-like initialization messages
// typed character by character, then the PicoTTL banner built from the
// machine's own character generator, framed and subtitled.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "pico/stdlib.h"

#include "DemoDraw.hpp"
#include "DemoText.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/Text.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"

namespace mda_demo {

// MDA backend pixel values (bit 0 = VIDEO, bit 1 = INTENSITY).
// Index 2 (intensity without video) is invisible on a 5151 - the
// monitor sums intensity onto the video beam - so the demo uses only
// black, normal and bright.
inline constexpr picottl::Color kBlack{0};
inline constexpr picottl::Color kNormal{1};
inline constexpr picottl::Color kBright{3};

inline const picottl::demo::ScreenColors kScreen = {
    kBlack,  // background
    kNormal, // frame
    kBright, // heading
    kNormal, // label
    kBright, // value
};

/// Blinks a block cursor at the current Text cursor cell.
inline void blinkCursorAt(picottl::Text& text, std::int32_t column,
                          std::int32_t row, int blinks,
                          std::uint32_t halfPeriodMs) {
    namespace demo = picottl::demo;
    for (int i = 0; i < blinks; ++i) {
        text.setCursor(column, row);
        text.putChar(demo::kBlock);
        sleep_ms(halfPeriodMs);
        text.setCursor(column, row);
        text.putChar(' ');
        sleep_ms(halfPeriodMs);
    }
}

inline void runIntro(picottl::Graphics& g, picottl::Text& text) {
    namespace demo = picottl::demo;

    // 1. A dark screen with a lone blinking cursor - the CRT moment
    //    every PC of the era began with.
    text.setColors(kNormal, kBlack);
    text.clear();
    blinkCursorAt(text, 0, 0, 3, 320);

    // 2. BIOS-like initialization messages, typed.
    text.setColors(kNormal, kBlack);
    text.setCursor(0, 0);
    demo::typeText(text, "PicoTTL Display Services\r\n", 12);
    demo::typeText(text, "Video: IBM Monochrome Display Adapter\r\n", 12);
    sleep_ms(250);
    text.print("720 x 350 raster ........ ");
    sleep_ms(400);
    demo::typeText(text, "OK\r\n", 30);
    text.print("Character generator 9x14  ");
    sleep_ms(400);
    demo::typeText(text, "OK\r\n", 30);
    sleep_ms(900);
    blinkCursorAt(text, 0, 5, 2, 320);
    sleep_ms(300);

    // 3. Banner: "PicoTTL" built from the MDA's own 9x14 glyphs,
    //    magnified 4x3 - letters appear one at a time.
    g.clear(kBlack);
    const auto& font = picottl::fonts::kIbmMda9x14;
    const char* kTitle = "PicoTTL";
    const std::int32_t scaleX = 4;
    const std::int32_t scaleY = 3;
    const std::int32_t bannerWidth = demo::bigTextWidth(font, kTitle, scaleX);
    const std::int32_t bannerX = (g.width() - bannerWidth) / 2;
    const std::int32_t bannerY = 96;
    {
        char letter[2] = {0, 0};
        std::int32_t penX = bannerX;
        for (const char* p = kTitle; *p != '\0'; ++p) {
            letter[0] = *p;
            demo::drawBigText(g, font, letter, penX, bannerY, scaleX, scaleY,
                              kBright);
            penX += font.glyphWidth * scaleX;
            sleep_ms(160);
        }
    }
    // A rule sweeping in beneath the banner.
    for (std::int32_t w = 0; w <= bannerWidth; w += 12) {
        g.drawHorizontalLine(bannerX, bannerY + font.glyphHeight * scaleY + 8,
                             w, kNormal);
        sleep_ms(8);
    }

    // 4. Typed subtitle and frame.
    text.setColors(kNormal, kBlack);
    demo::typeCentered(text, 14, "IBM Monochrome Display Adapter", 22);
    demo::typeCentered(text, 16, "D e m o n s t r a t i o n", 22);
    sleep_ms(400);
    text.setColors(kNormal, kBlack);
    demo::drawBox(text, 14, 4, 52, 16, demo::BoxStyle::Double);
    text.setColors(kNormal, kBlack);
    demo::printCentered(text, 21, "PicoTTL drives this monitor from a");
    demo::printCentered(text, 22, "Raspberry Pi Pico 2 - no PC required");
    sleep_ms(3200);

    demo::wipeDown(g, kBlack, 14, 12);
}

} // namespace mda_demo
