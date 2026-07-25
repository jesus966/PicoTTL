// PicoTTL - Demonstration 97: IBM Color Graphics Adapter (scenes).
//
// The demonstration scenes: the sixteen RGBI colors, the historical
// 320x200 palettes, geometric primitives, ordered dithering, a
// composed sunset scene, sprite animation and color transitions.
// Each scene draws itself, holds for a moment and returns; main.cpp
// sequences them with transitions.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

#include "pico/stdlib.h"

#include "DemoDraw.hpp"
#include "DemoText.hpp"
#include "Intro.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/Text.hpp"
#include "picottl/ega/Colors.hpp"

namespace cga_demo {

namespace demo = picottl::demo;

// CGA pixels are much taller than wide (640x200 stretched to 4:3);
// visually round shapes need rx = ry * 12 / 5.
inline constexpr std::int32_t roundRx(std::int32_t ry) {
    return ry * 12 / 5;
}

/// Standard scene header: white title on row 0 over a rule.
inline void sceneHeader(picottl::Text& text, const char* title) {
    text.setColors(cga::kWhite, cga::kBlack);
    text.clear();
    demo::printCentered(text, 0, title);
    text.setColors(cga::kLightCyan, cga::kBlack);
    text.setCursor(0, 1);
    for (std::int32_t i = 0; i < text.columns(); ++i) {
        text.putChar(static_cast<char>(0xC4));
    }
}

// ---------------------------------------------------------------------------
// Scene: all sixteen RGBI colors, on screen at once.
// ---------------------------------------------------------------------------
inline void sceneRgbi(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "T H E   S I X T E E N   R G B I   C O L O R S");

    static const char* kNames[16] = {
        "0 black",  "1 blue",   "2 green",  "3 cyan",
        "4 red",    "5 magnta", "6 brown",  "7 lt gry",
        "8 dk gry", "9 lt blu", "A lt grn", "B lt cyn",
        "C lt red", "D lt mag", "E yellow", "F white",
    };

    for (int i = 0; i < 16; ++i) {
        const std::int32_t x = 20 + (i % 8) * 76;
        const std::int32_t y = i < 8 ? 32 : 112;
        const picottl::Color color = cga::kRgbi[i];
        demo::fillRect(g, x, y, 68, 48, color);
        demo::drawRect(g, x, y, 68, 48, cga::kDarkGray);
        text.setColors(cga::kLightGray, cga::kBlack);
        text.setCursor(x / 8, y / 8 + 7);
        text.print(kNames[i]);
        sleep_ms(90);
    }

    text.setColors(cga::kLightGray, cga::kBlack);
    demo::printCentered(text, 23,
                        "All sixteen colors on screen at once - the full "
                        "RGBI gamut");
    sleep_ms(4800);
}

// ---------------------------------------------------------------------------
// Scene: the historical four-color palettes of the 320x200 modes.
// ---------------------------------------------------------------------------
inline void scenePalettes(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "T H E   3 2 0 x 2 0 0   P A L E T T E S");

    struct Panel {
        const char* title;
        const picottl::Color* palette;
        std::int32_t x, y;
    };
    const Panel panels[4] = {
        {"PALETTE 0 - LOW", cga::kPalette0, 24, 32},
        {"PALETTE 0 - HIGH", cga::kPalette0High, 328, 32},
        {"PALETTE 1 - LOW", cga::kPalette1, 24, 112},
        {"PALETTE 1 - HIGH", cga::kPalette1High, 328, 112},
    };

    for (const Panel& panel : panels) {
        text.setColors(cga::kWhite, cga::kBlack);
        text.setCursor(panel.x / 8, panel.y / 8);
        text.print(panel.title);
        for (int i = 0; i < 4; ++i) {
            const std::int32_t x = panel.x + i * 72;
            const std::int32_t y = panel.y + 12;
            demo::fillRect(g, x, y, 64, 40, panel.palette[i]);
            demo::drawRect(g, x, y, 64, 40, cga::kDarkGray);
        }
        sleep_ms(500);
    }

    text.setColors(cga::kLightGray, cga::kBlack);
    demo::printCentered(text, 23,
                        "Period palettes are data, not modes - Packed4 "
                        "scanout frees all sixteen");
    sleep_ms(4800);
}

// ---------------------------------------------------------------------------
// Scene: geometric primitives, drawn progressively.
// ---------------------------------------------------------------------------
inline void scenePrimitives(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "G E O M E T R I C   P R I M I T I V E S");

    text.setColors(cga::kLightGray, cga::kBlack);
    text.setCursor(9, 3);
    text.print("OUTLINES");
    text.setCursor(36, 3);
    text.print("FILLED");
    text.setCursor(61, 3);
    text.print("LINES");

    // Outlines.
    demo::drawRect(g, 32, 44, 140, 56, cga::kLightCyan);
    sleep_ms(300);
    demo::drawRect(g, 52, 60, 140, 56, cga::kLightGreen);
    sleep_ms(300);
    demo::drawEllipse(g, 112, 140, roundRx(24), 24, cga::kYellow);
    sleep_ms(300);
    demo::drawEllipse(g, 112, 140, roundRx(14), 14, cga::kLightRed);
    sleep_ms(400);

    // Filled.
    demo::fillRect(g, 244, 44, 120, 48, cga::kRed);
    sleep_ms(300);
    demo::fillRect(g, 276, 68, 120, 48, cga::kBlue);
    sleep_ms(300);
    demo::fillEllipse(g, 320, 148, roundRx(22), 22, cga::kLightMagenta);
    sleep_ms(300);
    demo::fillEllipse(g, 320, 148, roundRx(11), 11, cga::kWhite);
    sleep_ms(400);

    // A line fan.
    for (int i = 0; i <= 12; ++i) {
        const std::int32_t x = 456 + i * 13;
        g.drawLine(456, 168, x, 40,
                   (i & 1) ? cga::kLightBlue : cga::kLightCyan);
        sleep_ms(70);
    }
    for (int i = 0; i <= 10; ++i) {
        g.drawLine(456, 168, 612, 40 + i * 13,
                   (i & 1) ? cga::kMagenta : cga::kLightMagenta);
        sleep_ms(70);
    }

    text.setColors(cga::kLightGray, cga::kBlack);
    demo::printCentered(text, 23,
                        "Lines, rectangles and aspect-corrected circles "
                        "on 640 x 200");
    sleep_ms(4200);
}

// ---------------------------------------------------------------------------
// Scene: ordered dithering - many more shades than sixteen.
// ---------------------------------------------------------------------------
inline void sceneDither(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "O R D E R E D   D I T H E R I N G");

    // Left: a smooth sky ramp from five colors.
    static const picottl::Color kSky[5] = {
        cga::kBlack, cga::kBlue, cga::kLightBlue, cga::kLightCyan,
        cga::kWhite,
    };
    demo::ditherVGradient(g, 24, 32, 280, 136, kSky, 5);
    demo::drawRect(g, 24, 32, 280, 136, cga::kDarkGray);
    text.setColors(cga::kLightGray, cga::kBlack);
    text.setCursor(3, 22);
    text.print("Five colors, one smooth ramp");

    // Right: 50% mixes create colors CGA never had.
    struct Mix {
        picottl::Color a, b;
        const char* name;
    };
    const Mix mixes[4] = {
        {cga::kRed, cga::kYellow, "red + yellow   = orange"},
        {cga::kLightRed, cga::kWhite, "lt red + white = pink"},
        {cga::kBlue, cga::kLightCyan, "blue + lt cyan = teal"},
        {cga::kGreen, cga::kBrown, "green + brown  = olive"},
    };
    for (int i = 0; i < 4; ++i) {
        const std::int32_t y = 32 + i * 36;
        demo::ditherRect(g, 336, y, 168, 24, mixes[i].a, mixes[i].b, 32);
        demo::drawRect(g, 336, y, 168, 24, cga::kDarkGray);
        text.setColors(cga::kLightGray, cga::kBlack);
        text.setCursor(64, y / 8 + 1);
        text.printf("%s", mixes[i].name);
        sleep_ms(350);
    }

    text.setColors(cga::kLightGray, cga::kBlack);
    demo::printCentered(text, 23,
                        "The 8x8 Bayer matrix multiplies sixteen colors "
                        "into many more");
    sleep_ms(4800);
}

// ---------------------------------------------------------------------------
// Scene: a composed sunset - limitations turned into a style.
// ---------------------------------------------------------------------------
inline void sceneSunset(picottl::Graphics& g, picottl::Text& text) {
    text.setColors(cga::kWhite, cga::kBlack);
    text.clear();

    // Sky, dusk to horizon.
    static const picottl::Color kSky[4] = {
        cga::kBlue, cga::kMagenta, cga::kLightRed, cga::kYellow,
    };
    demo::ditherVGradient(g, 0, 0, 640, 120, kSky, 4);

    // The sun, half set.
    demo::fillEllipse(g, 320, 118, roundRx(26), 26, cga::kYellow);
    demo::fillEllipse(g, 320, 112, roundRx(13), 13, cga::kWhite);

    // The sea.
    static const picottl::Color kSea[3] = {
        cga::kLightRed, cga::kMagenta, cga::kBlue,
    };
    demo::ditherVGradient(g, 0, 120, 640, 72, kSea, 3);

    // The sun's reflection, breaking up with distance.
    for (std::int32_t y = 124; y <= 168; y += 4) {
        const std::int32_t half = (56 * (168 - y)) / 44 + 6;
        const std::int32_t offset = ((y >> 2) & 1) * 10 - 5;
        g.drawHorizontalLine(320 - half + offset, y, half * 2,
                             cga::kYellow);
    }

    // Distant birds.
    const std::int32_t birds[3][2] = {{150, 48}, {182, 40}, {470, 56}};
    for (const auto& b : birds) {
        g.drawLine(b[0] - 6, b[1], b[0], b[1] - 4, cga::kBlack);
        g.drawLine(b[0], b[1] - 4, b[0] + 6, b[1], cga::kBlack);
    }

    text.setColors(cga::kWhite, cga::kBlack);
    demo::fillTextRect(text, 0, 24, 80, 1, ' ');
    demo::printCentered(text, 24,
                        "Sixteen colors, ordered dither - and nothing else");
    sleep_ms(6000);
}

// ---------------------------------------------------------------------------
// Scene: sprite animation.
// ---------------------------------------------------------------------------
namespace sprites {

inline constexpr std::uint8_t kInvaderData[] = {
    0x0E, 0x00, // ....###....
    0x5F, 0x40, // .#.#####.#.
    0x7F, 0xC0, // .#########.
    0xDF, 0x60, // ##.#####.##
    0xFF, 0xE0, // ###########
    0x35, 0x80, // ..##.#.##..
    0x4A, 0x40, // .#..#.#..#.
    0xC0, 0x60, // ##.......##
};
inline constexpr picottl::MonochromeBitmap kInvader{kInvaderData, 11, 8, 0};

/// Draws the invader with 2x horizontal magnification (compensating
/// the tall CGA pixels), transparent background.
inline void drawInvader(picottl::Graphics& g, std::int32_t x,
                        std::int32_t y, picottl::Color color) {
    for (std::int32_t row = 0; row < kInvader.height; ++row) {
        const std::uint8_t* bits =
            kInvader.data + row * kInvader.effectiveStrideBytes();
        for (std::int32_t col = 0; col < kInvader.width; ++col) {
            if ((bits[col >> 3] >> (7 - (col & 7))) & 1u) {
                g.drawHorizontalLine(x + col * 2, y + row, 2, color);
            }
        }
    }
}

} // namespace sprites

inline void sceneSprites(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "S P R I T E   A N I M A T I O N");

    text.setColors(cga::kLightCyan, cga::kBlack);
    demo::drawBox(text, 2, 3, 76, 19, demo::BoxStyle::Single);
    text.setColors(cga::kLightGray, cga::kBlack);
    demo::printCentered(text, 23,
                        "Software sprites - erased and redrawn every frame");

    struct Sprite {
        std::int32_t x, y, dx, dy;
        picottl::Color color;
    };
    Sprite s[3] = {
        {60, 50, 3, 1, cga::kLightGreen},
        {300, 90, -3, 1, cga::kLightCyan},
        {480, 130, 3, -1, cga::kLightMagenta},
    };
    const std::int32_t minX = 26, maxX = 606 - 22;
    const std::int32_t minY = 34, maxY = 166 - 8;

    for (int frame = 0; frame < 160; ++frame) {
        for (Sprite& sp : s) {
            demo::fillRect(g, sp.x, sp.y, 22, 8, cga::kBlack); // erase
            sp.x += sp.dx;
            sp.y += sp.dy;
            if (sp.x <= minX || sp.x >= maxX) {
                sp.dx = -sp.dx;
                sp.x += sp.dx;
            }
            if (sp.y <= minY || sp.y >= maxY) {
                sp.dy = -sp.dy;
                sp.y += sp.dy;
            }
            sprites::drawInvader(g, sp.x, sp.y, sp.color);
        }
        sleep_ms(55);
    }
    sleep_ms(500);
}

// ---------------------------------------------------------------------------
// Scene: color transitions - the sixteen colors in motion.
// ---------------------------------------------------------------------------
inline void sceneColorCycle(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "C O L O R   T R A N S I T I O N S");
    text.setColors(cga::kLightGray, cga::kBlack);
    demo::printCentered(text, 23,
                        "Sixteen bars, one step per frame - pure RGBI "
                        "in motion");

    for (int t = 0; t < 48; ++t) {
        for (int i = 0; i < 16; ++i) {
            demo::fillRect(g, i * 40, 24, 40, 152,
                           cga::kRgbi[(i + t) & 15]);
        }
        sleep_ms(110);
    }
    sleep_ms(400);
}

} // namespace cga_demo
