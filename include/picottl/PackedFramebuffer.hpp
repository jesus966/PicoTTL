// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// PackedFramebuffer.hpp
// Generic packed-pixel framebuffer over caller-provided memory.
//
// PUBLIC API - FROZEN. Platform-independent.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "picottl/Framebuffer.hpp"

namespace picottl {

/// The primary framebuffer implementation of PicoTTL.
///
/// Pixels are stored as BitsPerPixel-wide fields, row-major, MSB-first
/// within each byte, rows padded to whole bytes. The low BitsPerPixel
/// bits of each Color index are stored verbatim: this class assigns NO
/// meaning to pixel values - only the active display backend interprets
/// them (electrically or otherwise).
///
/// Supported depths: 1, 2, 4, 6 and 8 bits per pixel. Whether a given
/// backend can scan out a given format is a backend property validated
/// at begin(), never an architectural limit.
///
/// Usage:
///   alignas(std::uint32_t) static std::uint8_t storage
///       [picottl::PackedFramebuffer2::bytesFor(720, 350)];
///   picottl::PackedFramebuffer2 framebuffer(storage, sizeof storage,
///                                           720, 350);
///
/// Note: backends that scan the framebuffer out by DMA require 4-byte
/// aligned storage (use alignas as above). The framework never
/// allocates: the application provides (and owns) the storage.
template <unsigned BitsPerPixel>
class PackedFramebuffer final : public Framebuffer {
    static_assert(BitsPerPixel == 1 || BitsPerPixel == 2 ||
                      BitsPerPixel == 4 || BitsPerPixel == 6 ||
                      BitsPerPixel == 8,
                  "supported packed depths: 1, 2, 4, 6 and 8 bits per pixel");

public:
    /// Pixel format identifier corresponding to BitsPerPixel.
    static constexpr PixelFormat kPixelFormat =
        BitsPerPixel == 1   ? PixelFormat::Mono1
        : BitsPerPixel == 2 ? PixelFormat::Packed2
        : BitsPerPixel == 4 ? PixelFormat::Packed4
        : BitsPerPixel == 6 ? PixelFormat::Packed6
                            : PixelFormat::Packed8;

    /// Storage bytes required for a given size. Usable in constant
    /// expressions to size static buffers.
    static constexpr std::size_t bytesFor(std::uint16_t width,
                                          std::uint16_t height) {
        return (static_cast<std::size_t>(width) * BitsPerPixel + 7u) / 8u *
               height;
    }

    /// Wraps caller-provided storage. If the storage is null or smaller
    /// than bytesFor(width, height), the framebuffer is empty (0 x 0)
    /// and all drawing operations become no-ops.
    PackedFramebuffer(std::uint8_t* storage, std::size_t storageBytes,
                      std::uint16_t width, std::uint16_t height) {
        if (storage == nullptr || width == 0 || height == 0 ||
            storageBytes < bytesFor(width, height)) {
            return; // Remains empty; all operations are no-ops.
        }
        storage_ = storage;
        width_ = width;
        height_ = height;
        strideBytes_ = static_cast<std::uint16_t>(
            (static_cast<std::size_t>(width) * BitsPerPixel + 7u) / 8u);
    }

    std::uint16_t width() const override { return width_; }
    std::uint16_t height() const override { return height_; }

    void setPixel(std::uint16_t x, std::uint16_t y, Color color) override {
        if (x >= width_ || y >= height_) {
            return;
        }
        const std::size_t bitIndex =
            static_cast<std::size_t>(x) * BitsPerPixel;
        std::uint8_t* p = rowStart(y) + (bitIndex >> 3);
        const unsigned offset = static_cast<unsigned>(bitIndex & 7u);
        const std::uint8_t value = color.index & kValueMask;
        if (offset + BitsPerPixel <= 8u) {
            const unsigned shift = 8u - BitsPerPixel - offset;
            *p = static_cast<std::uint8_t>(
                (*p & ~(kValueMask << shift)) | (value << shift));
        } else {
            // Field crosses a byte boundary (6 bpp only).
            const unsigned spill = offset + BitsPerPixel - 8u;
            p[0] = static_cast<std::uint8_t>(
                (p[0] & ~(kValueMask >> spill)) | (value >> spill));
            const std::uint8_t lowMask = static_cast<std::uint8_t>(
                ((1u << spill) - 1u) << (8u - spill));
            p[1] = static_cast<std::uint8_t>(
                (p[1] & ~lowMask) |
                ((static_cast<unsigned>(value) << (8u - spill)) & 0xFFu));
        }
    }

    Color pixel(std::uint16_t x, std::uint16_t y) const override {
        if (x >= width_ || y >= height_) {
            return colors::kBlack;
        }
        const std::size_t bitIndex =
            static_cast<std::size_t>(x) * BitsPerPixel;
        const std::uint8_t* p = rowStart(y) + (bitIndex >> 3);
        const unsigned offset = static_cast<unsigned>(bitIndex & 7u);
        if (offset + BitsPerPixel <= 8u) {
            const unsigned shift = 8u - BitsPerPixel - offset;
            return Color{
                static_cast<std::uint8_t>((*p >> shift) & kValueMask)};
        }
        const unsigned spill = offset + BitsPerPixel - 8u;
        const unsigned high = (p[0] & (kValueMask >> spill)) << spill;
        const unsigned low = p[1] >> (8u - spill);
        return Color{static_cast<std::uint8_t>(high | low)};
    }

    void fill(Color color) override {
        if (storage_ == nullptr) {
            return;
        }
        for (std::uint16_t x = 0; x < width_; ++x) {
            setPixel(x, 0, color);
        }
        for (std::uint16_t y = 1; y < height_; ++y) {
            std::memcpy(rowStart(y), storage_, strideBytes_);
        }
    }

    void copyRows(std::uint16_t srcY, std::uint16_t dstY,
                  std::uint16_t rowCount) override {
        if (storage_ == nullptr || srcY == dstY || rowCount == 0 ||
            srcY >= height_ || dstY >= height_) {
            return;
        }
        const std::uint16_t maxRows =
            static_cast<std::uint16_t>(height_ - (srcY > dstY ? srcY : dstY));
        if (rowCount > maxRows) {
            rowCount = maxRows;
        }
        // Whole rows are contiguous byte runs, so overlapping moves are
        // a single memmove - this is what makes scrolling fast.
        std::memmove(rowStart(dstY), rowStart(srcY),
                     static_cast<std::size_t>(rowCount) * strideBytes_);
    }

    PixelFormat pixelFormat() const override { return kPixelFormat; }
    const std::uint8_t* data() const override { return storage_; }
    std::size_t sizeBytes() const override {
        return static_cast<std::size_t>(strideBytes_) * height_;
    }

    /// Raw mutable storage access (tests and inspection).
    std::uint8_t* data() { return storage_; }

    /// Bytes per row.
    std::size_t strideBytes() const { return strideBytes_; }

    /// True when the constructor accepted the storage.
    bool isValid() const { return storage_ != nullptr; }

private:
    static constexpr std::uint8_t kValueMask =
        static_cast<std::uint8_t>((1u << BitsPerPixel) - 1u);

    std::uint8_t* rowStart(std::uint16_t y) {
        return storage_ + static_cast<std::size_t>(y) * strideBytes_;
    }
    const std::uint8_t* rowStart(std::uint16_t y) const {
        return storage_ + static_cast<std::size_t>(y) * strideBytes_;
    }

    std::uint8_t* storage_ = nullptr;
    std::uint16_t width_ = 0;
    std::uint16_t height_ = 0;
    std::uint16_t strideBytes_ = 0;
};

/// Aliases for common formats.
using MonochromeFramebuffer = PackedFramebuffer<1>;
using PackedFramebuffer2 = PackedFramebuffer<2>;
using PackedFramebuffer4 = PackedFramebuffer<4>;
using PackedFramebuffer8 = PackedFramebuffer<8>;

} // namespace picottl
