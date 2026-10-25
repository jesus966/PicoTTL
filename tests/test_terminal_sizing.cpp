// SPDX-License-Identifier: MIT
#include "framework/test.hpp"
#include "picottl/DisplaySurface.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/Terminal.hpp"

namespace {
using picottl::Terminal;
struct Case { std::int32_t columns, rows; std::uint64_t expected; };
constexpr Case cases[] = {
    {80, 25, 2000}, {1, 1, 1}, {0, 25, 0}, {25, 0, 0}, {0, 0, 0},
    {-1, 25, 0}, {25, -1, 0}, {INT32_MIN, INT32_MAX, 0},
    {INT32_MAX, INT32_MIN, 0}, {INT32_MIN, INT32_MIN, 0},
    {65535, 65535, 4294836225ULL},
    {2, INT32_MAX, 4294967294ULL}, {65535, 65537, 4294967295ULL},
#if SIZE_MAX == UINT32_MAX
    {65536, 65536, 0}, {INT32_MAX, INT32_MAX, 0},
#else
    // Every positive int32_t product fits a 64-bit size_t. Neither a
    // SIZE_MAX product nor a larger one is reachable through this API.
    {65536, 65536, 4294967296ULL},
    {INT32_MAX, INT32_MAX, 4611686014132420609ULL},
#endif
};
constexpr bool countsMatch() {
    for (const auto& item : cases) {
        if (Terminal::cellsFor(item.columns, item.rows) != item.expected) return false;
    }
    return true;
}
static_assert(countsMatch(), "Cell counts must be exact or explicitly unrepresentable");

// Exposes the largest live Text grid without allocating pixel storage.
class LargeSurface final : public picottl::DisplaySurface {
public:
    std::uint16_t width() const override { return UINT16_MAX; }
    std::uint16_t height() const override { return UINT16_MAX; }
    void setPixel(std::uint16_t, std::uint16_t, picottl::Color) override {}
    picottl::Color pixel(std::uint16_t, std::uint16_t) const override { return {}; }
    void fill(picottl::Color) override {}
};
} // namespace

PICOTTL_TEST(terminalCellCountsMatchPlatformRepresentability) {
    for (const auto& item : cases) {
        CHECK_EQ(Terminal::cellsFor(item.columns, item.rows), item.expected);
    }
}

PICOTTL_TEST(terminalRejectsImpossibleCellStorageBeforeWriting) {
    LargeSurface surface;
    picottl::Graphics graphics(&surface);
    const std::uint8_t glyph = 0;
    const picottl::Font font{&glyph, 1, 1, 1, 1};
    picottl::Text text(graphics, font);
    CHECK_EQ(text.columns(), 65535);
    CHECK_EQ(text.rows(), 65535);
    picottl::TerminalCell cell;
    cell.character = 'Q';
    Terminal tooSmall(text, &cell, 1);
    CHECK(!tooSmall.isValid());
#if SIZE_MAX == UINT32_MAX
    // No array could back this claimed capacity: reject the byte-size
    // overflow before attempting initialization. Executable on 32-bit builds.
    Terminal impossible(text, &cell, SIZE_MAX);
    CHECK(!impossible.isValid());
    impossible.print("inert");
#endif
    CHECK_EQ(cell.character, 'Q');
}
