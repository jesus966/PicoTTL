// PicoTTL - host tests: PackedFramebuffer
//
// Validates packing exactness at every supported depth, clipping,
// fill patterns, copyRows overlap semantics and invalid-storage
// behaviour - the same guarantees Example 07/20/21 validate on the
// real IBM 5151, here as automated regressions.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <vector>

#include "framework/test.hpp"
#include "picottl/PackedFramebuffer.hpp"

using picottl::Color;

PICOTTL_TEST(bytesForMatchesKnownSizes) {
    CHECK_EQ(picottl::MonochromeFramebuffer::bytesFor(720, 350), 31500u);
    CHECK_EQ(picottl::PackedFramebuffer2::bytesFor(720, 350), 63000u);
    CHECK_EQ(picottl::PackedFramebuffer4::bytesFor(720, 350), 126000u);
    CHECK_EQ(picottl::PackedFramebuffer2::bytesFor(7, 3), 6u); // padded rows
}

PICOTTL_TEST(monochromePackingIsMsbFirst) {
    std::uint8_t storage[picottl::MonochromeFramebuffer::bytesFor(16, 2)] = {};
    picottl::MonochromeFramebuffer fb(storage, sizeof storage, 16, 2);
    for (std::uint16_t x = 0; x < 16; x += 2) {
        fb.setPixel(x, 0, Color{1}); // 10101010...
    }
    CHECK_EQ(storage[0], 0xAA);
    CHECK_EQ(storage[1], 0xAA);
    CHECK_EQ(storage[2], 0x00); // second row untouched
}

PICOTTL_TEST(packed2PackingIsMsbFirst) {
    std::uint8_t storage[picottl::PackedFramebuffer2::bytesFor(8, 1)] = {};
    picottl::PackedFramebuffer2 fb(storage, sizeof storage, 8, 1);
    for (std::uint16_t x = 0; x < 4; ++x) {
        fb.setPixel(x, 0, Color{static_cast<std::uint8_t>(x)});
    }
    CHECK_EQ(storage[0], 0x1B); // 00 01 10 11
}

PICOTTL_TEST(packed4PackingIsMsbFirst) {
    std::uint8_t storage[picottl::PackedFramebuffer4::bytesFor(4, 1)] = {};
    picottl::PackedFramebuffer4 fb(storage, sizeof storage, 4, 1);
    fb.setPixel(0, 0, Color{0xA});
    fb.setPixel(1, 0, Color{0x5});
    fb.setPixel(2, 0, Color{0xF});
    fb.setPixel(3, 0, Color{0x1});
    CHECK_EQ(storage[0], 0xA5);
    CHECK_EQ(storage[1], 0xF1);
}

PICOTTL_TEST(pixelRoundTripAllValuesAllDepths) {
    {
        std::uint8_t storage[picottl::PackedFramebuffer2::bytesFor(9, 2)] = {};
        picottl::PackedFramebuffer2 fb(storage, sizeof storage, 9, 2);
        for (std::uint16_t x = 0; x < 9; ++x) {
            const Color c{static_cast<std::uint8_t>((x + 1) & 3)};
            fb.setPixel(x, 1, c);
            CHECK_EQ(fb.pixel(x, 1).index, c.index);
        }
    }
    {
        std::uint8_t storage[picottl::PackedFramebuffer4::bytesFor(5, 1)] = {};
        picottl::PackedFramebuffer4 fb(storage, sizeof storage, 5, 1);
        for (std::uint16_t x = 0; x < 5; ++x) {
            const Color c{static_cast<std::uint8_t>((x * 3 + 7) & 15)};
            fb.setPixel(x, 0, c);
            CHECK_EQ(fb.pixel(x, 0).index, c.index);
        }
    }
}

PICOTTL_TEST(packed6CrossesByteBoundaries) {
    std::uint8_t storage[picottl::PackedFramebuffer<6>::bytesFor(8, 2)] = {};
    picottl::PackedFramebuffer<6> fb(storage, sizeof storage, 8, 2);
    for (std::uint16_t x = 0; x < 8; ++x) {
        const Color c{static_cast<std::uint8_t>((x * 11 + 5) & 63)};
        fb.setPixel(x, 1, c);
        CHECK_EQ(fb.pixel(x, 1).index, c.index);
    }
    // Neighbours must survive rewrites (boundary-crossing fields).
    fb.setPixel(1, 1, Color{0});
    CHECK_EQ(fb.pixel(0, 1).index, (0 * 11 + 5) & 63);
    CHECK_EQ(fb.pixel(2, 1).index, (2 * 11 + 5) & 63);
}

PICOTTL_TEST(fillReplicatesPattern) {
    std::uint8_t storage[picottl::PackedFramebuffer2::bytesFor(16, 3)] = {};
    picottl::PackedFramebuffer2 fb(storage, sizeof storage, 16, 3);
    fb.fill(Color{2});
    for (std::size_t i = 0; i < sizeof storage; ++i) {
        CHECK_EQ(storage[i], 0xAA); // 10 replicated
    }
    fb.fill(Color{3});
    CHECK_EQ(storage[0], 0xFF);
}

PICOTTL_TEST(copyRowsHandlesOverlapBothDirections) {
    std::uint8_t storage[picottl::MonochromeFramebuffer::bytesFor(8, 6)] = {};
    picottl::MonochromeFramebuffer fb(storage, sizeof storage, 8, 6);
    for (std::uint16_t y = 0; y < 6; ++y) {
        storage[y] = static_cast<std::uint8_t>(0x10 + y);
    }
    fb.copyRows(2, 0, 4); // upwards, overlapping
    CHECK_EQ(storage[0], 0x12);
    CHECK_EQ(storage[3], 0x15);
    CHECK_EQ(storage[4], 0x14); // below the copy: untouched
    for (std::uint16_t y = 0; y < 6; ++y) {
        storage[y] = static_cast<std::uint8_t>(0x20 + y);
    }
    fb.copyRows(0, 2, 4); // downwards, overlapping
    CHECK_EQ(storage[2], 0x20);
    CHECK_EQ(storage[5], 0x23);
    CHECK_EQ(storage[0], 0x20); // above the copy: untouched
}

PICOTTL_TEST(copyRowsClipsRowCount) {
    std::uint8_t storage[picottl::MonochromeFramebuffer::bytesFor(8, 4)] = {};
    picottl::MonochromeFramebuffer fb(storage, sizeof storage, 8, 4);
    for (std::uint16_t y = 0; y < 4; ++y) {
        storage[y] = static_cast<std::uint8_t>(y + 1);
    }
    fb.copyRows(3, 1, 100); // only one row fits
    CHECK_EQ(storage[1], 4);
    CHECK_EQ(storage[2], 3);
}

PICOTTL_TEST(outOfRangeAccessIsClippedAndSafe) {
    std::uint8_t storage[picottl::PackedFramebuffer2::bytesFor(4, 2)] = {};
    picottl::PackedFramebuffer2 fb(storage, sizeof storage, 4, 2);
    fb.setPixel(4, 0, Color{3});
    fb.setPixel(0, 2, Color{3});
    for (std::size_t i = 0; i < sizeof storage; ++i) {
        CHECK_EQ(storage[i], 0x00);
    }
    CHECK_EQ(fb.pixel(4, 0).index, 0); // colors::kBlack
}

PICOTTL_TEST(insufficientStorageMakesFramebufferInert) {
    std::uint8_t storage[4] = {};
    picottl::PackedFramebuffer2 fb(storage, sizeof storage, 720, 350);
    CHECK(!fb.isValid());
    CHECK_EQ(fb.width(), 0);
    CHECK_EQ(fb.height(), 0);
    fb.setPixel(0, 0, Color{3}); // must be a safe no-op
    fb.fill(Color{3});
    CHECK_EQ(storage[0], 0x00);
}

namespace {
template <unsigned Depth>
void checkPackedCapacityBoundaries() {
    using Buffer = picottl::PackedFramebuffer<Depth>;
    const std::uint16_t widths[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17};
    const auto mask = static_cast<std::uint8_t>((1u << Depth) - 1u);
    for (const auto width : widths) for (std::uint16_t height = 1; height <= 3; ++height) {
        const auto required = Buffer::bytesFor(width, height);
        std::vector<std::uint8_t> storage(required + 2, 0xa5);
        // The byte-offset address is valid for CPU drawing at every depth.
        Buffer fb(storage.data() + 1, required, width, height);
        CHECK(fb.isValid()); CHECK_EQ(fb.sizeBytes(), required);
        CHECK_EQ(fb.strideBytes() * height, required);
        for (std::uint16_t y = 0; y < height; ++y)
            for (std::uint16_t x = 0; x < width; ++x)
                fb.setPixel(x, y, Color{static_cast<std::uint8_t>((x * 11u + y * 7u) & mask)});
        // Rewriting the last pixel must preserve every other field, including
        // neighbours sharing a byte and 6-bit fields crossing byte boundaries.
        fb.setPixel(width - 1, height - 1, Color{mask});
        for (std::uint16_t y = 0; y < height; ++y)
            for (std::uint16_t x = 0; x < width; ++x)
                CHECK_EQ(fb.pixel(x, y).index, x == width - 1 && y == height - 1
                    ? mask : static_cast<std::uint8_t>((x * 11u + y * 7u) & mask));
        fb.fill(Color{mask});
        fb.copyRows(0, height - 1, UINT16_MAX);
        for (std::uint16_t y = 0; y < height; ++y)
            for (std::uint16_t x = 0; x < width; ++x)
                CHECK_EQ(fb.pixel(x, y).index, mask);
        CHECK_EQ(storage.front(), 0xa5); CHECK_EQ(storage.back(), 0xa5);

        std::vector<std::uint8_t> shortStorage(required, 0x5a);
        Buffer shortBuffer(shortStorage.data(), required - 1, width, height);
        CHECK(!shortBuffer.isValid()); CHECK_EQ(shortBuffer.sizeBytes(), 0u);
        shortBuffer.setPixel(width - 1, height - 1, Color{mask});
        shortBuffer.fill(Color{mask}); shortBuffer.copyRows(0, 1, UINT16_MAX);
        CHECK_EQ(shortBuffer.pixel(0, 0).index, 0u);
        for (const auto byte : shortStorage) CHECK_EQ(byte, 0x5a);
        // Exact allocation gives ASan a redzone immediately after the frame.
        std::vector<std::uint8_t> exact(required);
        Buffer exactBuffer(exact.data(), exact.size(), width, height);
        exactBuffer.fill(Color{mask});
        CHECK_EQ(exactBuffer.pixel(width - 1, height - 1).index, mask);
    }
    std::uint8_t byte = 0x5a;
    Buffer noWidth(&byte, 1, 0, 1), noHeight(&byte, 1, 1, 0), noStorage(nullptr, 1, 1, 1);
    CHECK(!noWidth.isValid()); CHECK(!noHeight.isValid()); CHECK(!noStorage.isValid());
}
}

PICOTTL_TEST(packedCapacityBoundariesAtEveryDepth) {
    static_assert(picottl::PackedFramebuffer<1>::bytesFor(65535, 65535) == 536862720u);
    static_assert(picottl::PackedFramebuffer<2>::bytesFor(65535, 65535) == 1073725440u);
    static_assert(picottl::PackedFramebuffer<4>::bytesFor(65535, 65535) == 2147450880u);
    static_assert(picottl::PackedFramebuffer<6>::bytesFor(65535, 65535) == 3221176320u);
    static_assert(picottl::PackedFramebuffer<8>::bytesFor(65535, 65535) == 4294836225u);
    checkPackedCapacityBoundaries<1>(); checkPackedCapacityBoundaries<2>();
    checkPackedCapacityBoundaries<4>(); checkPackedCapacityBoundaries<6>();
    checkPackedCapacityBoundaries<8>();
}
