// PicoTTL - Demonstration 98: IBM Enhanced Graphics Adapter (scenes).
//
// The demonstration scenes: the sixty-four color gamut, palettes as a
// choice of 16 from 64, gun-level gradients, per-pixel shaded spheres,
// a composed vista, smooth full-width animation and a mixed
// text-and-graphics chart. Each scene draws itself, holds for a moment
// and returns; main.cpp sequences them with transitions.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <cstring>

#include "pico/stdlib.h"

#include "DemoDraw.hpp"
#include "DemoText.hpp"
#include "Intro.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/Text.hpp"
#include "picottl/ega/Colors.hpp"
#include "picottl/fonts/IbmEga8x14.hpp"

namespace ega_demo {

namespace demo = picottl::demo;

// EGA 350-line pixels are slightly taller than wide (640x350 stretched
// to 4:3); visually round shapes need rx = ry * 11 / 8.
inline constexpr std::int32_t roundRx(std::int32_t ry) {
    return ry * 11 / 8;
}

/// Draws a small pixel-positioned label (unaligned to the cell grid)
/// straight from the EGA character generator.
inline void drawPixelText(picottl::Graphics& g, std::int32_t x,
                          std::int32_t y, const char* s,
                          picottl::Color color) {
    const auto& font = picottl::fonts::kIbmEga8x14;
    for (const char* p = s; *p != '\0'; ++p) {
        g.drawBitmap(x, y, font.glyph(static_cast<unsigned char>(*p)),
                     color);
        x += font.glyphWidth;
    }
}

/// Perceived brightness of an EGA value: each gun contributes
/// 2 x primary + secondary (0..3), total 0..9.
inline std::int32_t gunBrightness(std::uint8_t value) {
    const std::int32_t red = 2 * ((value >> 2) & 1) + ((value >> 5) & 1);
    const std::int32_t green = 2 * ((value >> 1) & 1) + ((value >> 4) & 1);
    const std::int32_t blue = 2 * (value & 1) + ((value >> 3) & 1);
    return red + green + blue;
}

/// Standard scene header: white title on row 0 over a rule.
inline void sceneHeader(picottl::Text& text, const char* title) {
    text.setColors(ega::kWhite, ega::kBlack);
    text.clear();
    demo::printCentered(text, 0, title);
    text.setColors(ega::kLightBlue, ega::kBlack);
    text.setCursor(0, 1);
    for (std::int32_t i = 0; i < text.columns(); ++i) {
        text.putChar(static_cast<char>(0xC4));
    }
}

// ---------------------------------------------------------------------------
// Scene: all sixty-four colors, labeled, on screen at once.
// ---------------------------------------------------------------------------
inline void scene64Colors(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "S I X T Y - F O U R   C O L O R S");

    for (int value = 0; value < 64; ++value) {
        const std::int32_t x = 50 + (value % 8) * 68;
        const std::int32_t y = 36 + (value / 8) * 36;
        const picottl::Color color{static_cast<std::uint8_t>(value)};
        demo::fillRect(g, x, y, 64, 32, color);
        char label[3];
        label[0] = "0123456789ABCDEF"[value >> 4];
        label[1] = "0123456789ABCDEF"[value & 15];
        label[2] = '\0';
        drawPixelText(g, x + 4, y + 2, label,
                      gunBrightness(static_cast<std::uint8_t>(value)) >= 5
                          ? ega::kBlack
                          : ega::kWhite);
        if ((value & 7) == 7) {
            sleep_ms(120);
        }
    }

    text.setColors(ega::kLightGray, ega::kBlack);
    demo::printCentered(text, 24,
                        "Two levels per gun on six wires - all sixty-four "
                        "values in one frame");
    sleep_ms(5200);
}

// ---------------------------------------------------------------------------
// Scene: palettes - sixteen colors chosen from sixty-four.
// ---------------------------------------------------------------------------
inline void scenePalettes(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "S I X T E E N   F R O M   S I X T Y - F O U R");

    text.setColors(ega::kWhite, ega::kBlack);
    text.setCursor(4, 3);
    text.print("The power-on palette - the familiar CGA look:");
    for (int i = 0; i < 16; ++i) {
        const std::int32_t x = 34 + i * 36;
        demo::fillRect(g, x, 70, 32, 44, ega::kDefaultPalette[i]);
        demo::drawRect(g, x, 70, 32, 44, ega::kDarkGray);
        char label[2] = {"0123456789ABCDEF"[i], '\0'};
        drawPixelText(g, x + 12, 118, label, ega::kLightGray);
        sleep_ms(60);
    }

    text.setColors(ega::kWhite, ega::kBlack);
    text.setCursor(4, 11);
    text.print("...or any other sixteen - an amber palette no CGA "
               "could show:");
    // All sixteen red x green gun combinations (no blue): a warm ramp
    // that simply does not exist in the RGBI world.
    static constexpr std::uint8_t kRedLevel[4] = {0x00, 0x20, 0x04, 0x24};
    static constexpr std::uint8_t kGreenLevel[4] = {0x00, 0x10, 0x02, 0x12};
    for (int i = 0; i < 16; ++i) {
        const int red = i / 4;
        const int green = i % 4;
        const std::int32_t x = 34 + i * 36;
        const picottl::Color color{
            static_cast<std::uint8_t>(kRedLevel[red] | kGreenLevel[green])};
        demo::fillRect(g, x, 182, 32, 44, color);
        demo::drawRect(g, x, 182, 32, 44, ega::kDarkGray);
        sleep_ms(60);
    }

    text.setColors(ega::kLightGray, ega::kBlack);
    demo::printCentered(text, 18,
                        "(sixteen red x green gun combinations - "
                        "including the true EGA brown, 14h)");
    demo::printCentered(text, 24,
                        "On real hardware: sixteen palette registers, each "
                        "loadable with any of 64 values");
    sleep_ms(5600);
}

// ---------------------------------------------------------------------------
// Scene: gun levels and dithered gradients.
// ---------------------------------------------------------------------------
inline void sceneGradients(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "G U N   L E V E L S   A N D   G R A D I E N T S");

    struct Ramp {
        const char* name;
        picottl::Color levels[4];
    };
    static const Ramp ramps[4] = {
        {"GRAY", {picottl::Color{0x00}, picottl::Color{0x38},
                  picottl::Color{0x07}, picottl::Color{0x3F}}},
        {"RED", {picottl::Color{0x00}, picottl::Color{0x20},
                 picottl::Color{0x04}, picottl::Color{0x24}}},
        {"GREEN", {picottl::Color{0x00}, picottl::Color{0x10},
                   picottl::Color{0x02}, picottl::Color{0x12}}},
        {"BLUE", {picottl::Color{0x00}, picottl::Color{0x08},
                  picottl::Color{0x01}, picottl::Color{0x09}}},
    };

    drawPixelText(g, 112, 36, "FOUR HARDWARE LEVELS", ega::kLightGray);
    drawPixelText(g, 380, 36, "DITHERED TO A CONTINUOUS RAMP",
                  ega::kLightGray);

    for (int i = 0; i < 4; ++i) {
        const std::int32_t y = 62 + i * 62;
        drawPixelText(g, 40, y + 13, ramps[i].name, ega::kWhite);
        // Left half: the four raw levels, stepped.
        for (int level = 0; level < 4; ++level) {
            demo::fillRect(g, 112 + level * 59, y, 59, 40,
                           ramps[i].levels[level]);
        }
        // Right half: the same four levels, dithered smooth.
        demo::ditherHGradient(g, 348, y, 236, 40, ramps[i].levels, 4);
        demo::drawRect(g, 112, y, 472, 40, ega::kDarkGray);
        g.drawVerticalLine(348, y, 40, ega::kDarkGray);
        sleep_ms(350);
    }

    text.setColors(ega::kLightGray, ega::kBlack);
    demo::printCentered(text, 24,
                        "Two wires per gun give four levels - ordered "
                        "dither fills the gaps");
    sleep_ms(5200);
}

// ---------------------------------------------------------------------------
// Scene: per-pixel shaded spheres.
// ---------------------------------------------------------------------------
inline void shadedSphere(picottl::Graphics& g, std::int32_t cx,
                         std::int32_t cy, std::int32_t ry,
                         const picottl::Color* ramp, std::int32_t count) {
    const std::int32_t rx = roundRx(ry);
    const std::int32_t lightX = -(ry * 2) / 5; // in circle space
    const std::int32_t lightY = -(ry * 2) / 5;
    const std::int32_t maxDistance = (ry * 8) / 5;
    const std::int32_t maxProgress = (count - 1) * 64;
    for (std::int32_t dy = -ry; dy <= ry; ++dy) {
        const std::int64_t rest =
            static_cast<std::int64_t>(ry) * ry -
            static_cast<std::int64_t>(dy) * dy;
        const std::int32_t span = static_cast<std::int32_t>(demo::isqrt64(
            static_cast<std::uint64_t>(static_cast<std::int64_t>(rx) * rx *
                                       rest /
                                       (static_cast<std::int64_t>(ry) * ry))));
        for (std::int32_t dx = -span; dx <= span; ++dx) {
            // Normalize x into circle space, then shade by distance
            // from the light spot.
            const std::int32_t nx = dx * ry / rx - lightX;
            const std::int32_t ny = dy - lightY;
            const std::int32_t d = static_cast<std::int32_t>(demo::isqrt64(
                static_cast<std::uint64_t>(
                    static_cast<std::int64_t>(nx) * nx +
                    static_cast<std::int64_t>(ny) * ny)));
            std::int32_t progress =
                maxProgress * (maxDistance - d) / maxDistance;
            if (progress < 0) {
                progress = 0;
            }
            if (progress > maxProgress) {
                progress = maxProgress;
            }
            std::int32_t level = progress / 64;
            std::int32_t frac = progress % 64;
            if (level >= count - 1) {
                level = count - 2;
                frac = 64;
            }
            const std::int32_t px = cx + dx;
            const std::int32_t py = cy + dy;
            g.drawPixel(px, py,
                        frac > demo::bayer64(px, py) ? ramp[level + 1]
                                                     : ramp[level]);
        }
    }
}

inline void sceneSpheres(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "S H A D E D   S P H E R E S");

    static const picottl::Color kBlueRamp[5] = {
        picottl::Color{0x00}, picottl::Color{0x08}, picottl::Color{0x01},
        picottl::Color{0x09}, picottl::Color{0x3F},
    };
    static const picottl::Color kRedRamp[5] = {
        picottl::Color{0x00}, picottl::Color{0x20}, picottl::Color{0x04},
        picottl::Color{0x3C}, picottl::Color{0x3F},
    };
    static const picottl::Color kGrayRamp[4] = {
        picottl::Color{0x00}, picottl::Color{0x38}, picottl::Color{0x07},
        picottl::Color{0x3F},
    };

    shadedSphere(g, 190, 190, 78, kBlueRamp, 5);
    sleep_ms(250);
    shadedSphere(g, 390, 140, 52, kRedRamp, 5);
    sleep_ms(250);
    shadedSphere(g, 505, 240, 58, kGrayRamp, 4);

    text.setColors(ega::kLightGray, ega::kBlack);
    demo::printCentered(text, 24,
                        "Per-pixel shading: gun-level ramps blended with "
                        "the Bayer matrix");
    sleep_ms(5600);
}

// ---------------------------------------------------------------------------
// Scene: a composed vista - the CGA demo's sunset, revisited.
// ---------------------------------------------------------------------------
inline void sceneVista(picottl::Graphics& g, picottl::Text& text) {
    text.setColors(ega::kWhite, ega::kBlack);
    text.clear();

    // Sky, dusk to gold.
    static const picottl::Color kSky[6] = {
        picottl::Color{0x01}, picottl::Color{0x05}, picottl::Color{0x04},
        picottl::Color{0x24}, picottl::Color{0x34}, picottl::Color{0x3E},
    };
    demo::ditherVGradient(g, 0, 0, 640, 190, kSky, 6);

    // The sun with a layered halo.
    demo::fillEllipse(g, 320, 166, roundRx(40), 40, picottl::Color{0x34});
    demo::fillEllipse(g, 320, 164, roundRx(28), 28, picottl::Color{0x3E});
    demo::fillEllipse(g, 320, 162, roundRx(17), 17, picottl::Color{0x3F});

    // Two mountain ranges, silhouetted against the sky.
    for (std::int32_t x = 0; x < 640; ++x) {
        const std::int32_t farH = 26 + (demo::isin(x * 3 / 4 + 20) * 16) / 128 +
                                  (demo::isin(x * 3 + 90) * 5) / 128;
        g.drawVerticalLine(x, 190 - farH, farH, picottl::Color{0x28});
        const std::int32_t nearH = 30 + (demo::isin(x / 2 + 130) * 26) / 128 +
                                   (demo::isin(x * 2 + 35) * 8) / 128;
        g.drawVerticalLine(x, 190 - nearH, nearH, picottl::Color{0x08});
    }

    // The water.
    static const picottl::Color kSea[4] = {
        picottl::Color{0x34}, picottl::Color{0x05}, picottl::Color{0x01},
        picottl::Color{0x00},
    };
    demo::ditherVGradient(g, 0, 190, 640, 110, kSea, 4);

    // The sun's reflection.
    for (std::int32_t y = 194; y <= 252; y += 4) {
        const std::int32_t half = (52 * (252 - y)) / 58 + 6;
        const std::int32_t offset = ((y >> 2) & 1) * 12 - 6;
        g.drawHorizontalLine(320 - half + offset, y, half * 2,
                             picottl::Color{0x3E});
    }

    // Distant birds.
    const std::int32_t birds[3][2] = {{150, 70}, {184, 58}, {472, 84}};
    for (const auto& b : birds) {
        g.drawLine(b[0] - 7, b[1], b[0], b[1] - 5, picottl::Color{0x00});
        g.drawLine(b[0], b[1] - 5, b[0] + 7, b[1], picottl::Color{0x00});
    }

    demo::fillRect(g, 0, 300, 640, 50, ega::kBlack);
    text.setColors(ega::kLightGray, ega::kBlack);
    demo::printCentered(text, 23,
                        "The same sunset as the CGA demonstration -");
    text.setColors(ega::kWhite, ega::kBlack);
    demo::printCentered(text, 24,
                        "now composed from sixty-four colors");
    sleep_ms(6500);
}

// ---------------------------------------------------------------------------
// Scene: smooth full-width animation via hardware-speed row copies.
// ---------------------------------------------------------------------------
inline void sceneWaterfall(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "S M O O T H   A N I M A T I O N");
    text.setColors(ega::kLightGray, ega::kBlack);
    demo::printCentered(text, 24,
                        "280 scanlines scrolled every frame by row copies "
                        "in the framebuffer");

    static const std::uint8_t kWheel[20] = {
        0x01, 0x09, 0x0B, 0x3B, 0x03, 0x13, 0x3A, 0x12, 0x02, 0x06,
        0x16, 0x3E, 0x34, 0x24, 0x04, 0x3C, 0x0C, 0x05, 0x0D, 0x39,
    };
    const std::int32_t top = 28;
    const std::int32_t rows = 280; // region: y 28..307

    // Prime the region with color bands.
    for (std::int32_t band = 0; band < rows / 10; ++band) {
        demo::fillRect(g, 0, top + band * 10, 640, 10,
                       picottl::Color{kWheel[band % 20]});
    }

    int wheelIndex = rows / 10;
    for (int step = 0; step < 360; ++step) {
        g.copyRows(top + 2, top, rows - 2);
        demo::fillRect(g, 0, top + rows - 2, 640, 2,
                       picottl::Color{kWheel[wheelIndex % 20]});
        if (step % 5 == 4) {
            ++wheelIndex; // a fresh 10-pixel band every five steps
        }
        sleep_ms(25);
    }
    sleep_ms(400);
}

// ---------------------------------------------------------------------------
// Scene: mixed text and graphics - the leap, charted.
// ---------------------------------------------------------------------------
inline void sceneChart(picottl::Graphics& g, picottl::Text& text) {
    sceneHeader(text, "T H E   E G A   L E A P");

    // Axis.
    const std::int32_t baseY = 300;
    g.drawHorizontalLine(100, baseY, 460, ega::kLightGray);
    g.drawVerticalLine(100, 40, baseY - 40 + 1, ega::kLightGray);

    struct Bar {
        const char* name;
        const char* value;
        std::int32_t height; // 4 px per color value
        std::int32_t x;
    };
    const Bar bars[3] = {
        {"MDA", "4 shades", 16, 150},
        {"CGA", "16 colors", 64, 300},
        {"EGA", "64 colors", 256, 450},
    };

    for (int i = 0; i < 3; ++i) {
        const Bar& bar = bars[i];
        if (i < 2) {
            demo::fillRect(g, bar.x, baseY - bar.height, 90, bar.height,
                           i == 0 ? ega::kLightGray : ega::kCyan);
        } else {
            // The EGA bar carries its own gradient - of course.
            static const picottl::Color kBarRamp[4] = {
                picottl::Color{0x01}, picottl::Color{0x09},
                picottl::Color{0x3B}, picottl::Color{0x3F},
            };
            demo::ditherVGradient(g, bar.x, baseY - bar.height, 90,
                                  bar.height, kBarRamp, 4);
        }
        demo::drawRect(g, bar.x, baseY - bar.height, 90, bar.height,
                       ega::kDarkGray);
        drawPixelText(g, bar.x + 45 - 4 * static_cast<std::int32_t>(
                                              std::strlen(bar.value)),
                      baseY - bar.height - 20, bar.value, ega::kWhite);
        drawPixelText(g, bar.x + 45 - 4 * static_cast<std::int32_t>(
                                              std::strlen(bar.name)),
                      baseY + 8, bar.name, ega::kLightGray);
        sleep_ms(500);
    }

    text.setColors(ega::kLightGray, ega::kBlack);
    demo::printCentered(text, 24,
                        "Simultaneous colors per IBM display adapter, "
                        "1981 to 1984");
    sleep_ms(5200);
}

} // namespace ega_demo
