// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// MdaDisplayDevice.hpp
// IBM MDA display backend for the RP2350.
//
// PUBLIC API - applications construct this device, hand it to
// picottl::Display, and never touch it again. Only the Config struct
// and the DisplayDevice interface are application-facing; everything
// else in this backend is an implementation detail.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "picottl/DisplayDevice.hpp"
#include "picottl/Framebuffer.hpp"
#include "picottl/rp2350/PioScanlineEngine.hpp" // internal engine (by-value member)
#include "picottl/video/ModeTimings.hpp"        // backend SPI

namespace picottl::rp2350 {

/// IBM MDA-family display backend for the RP2350.
///
/// Implements the MDA standard family; currently the single curated
/// mode is VideoMode::IbmMda. Future MDA-family modes (overscan, full
/// raster, diagnostics) are added inside this backend without public
/// API changes.
///
/// Usage:
///   MdaDisplayDevice::Config config;
///   config.hsyncPin = 2;          // VSYNC on GPIO 3, GPIO 4 internal
///   config.videoPin = 5;
///   config.framebuffer = &framebuffer;
///   MdaDisplayDevice device(config);
///   picottl::Display display(device);
///   display.begin(picottl::VideoMode::IbmMda);
///
/// RAM note: this object embeds the fixed-size signal table the backend
/// needs (~5.8 KB), visible in the map file as part of the object. The
/// framebuffer memory is provided (and owned) by the application.
class MdaDisplayDevice final : public DisplayDevice {
public:
    /// Hardware resources used by this backend. The application decides;
    /// the framework validates and configures exactly these resources
    /// and claims nothing else.
    struct Config {
        /// Maximum pixel-value bits of any format (capacity of pixelPins).
        static constexpr std::size_t kMaxPixelBits = 8;

        std::uint8_t pioInstance = 0;       ///< PIO block index (0..2).
        std::uint8_t stateMachine = 0;      ///< Signal state machine (0..3).
        std::uint8_t pixelStateMachine = 1; ///< Pixel state machine (0..3).
        std::uint8_t dmaDataChannel = 0;    ///< DMA channel pair for signal
        std::uint8_t dmaControlChannel = 1; ///< generation.
        std::uint8_t pixelDmaDataChannel = 2;    ///< DMA channel pair for
        std::uint8_t pixelDmaControlChannel = 3; ///< pixel streaming.
        std::uint8_t hsyncPin = 0; ///< HSYNC GPIO (DE-9 pin 8). VSYNC is
                                   ///< hsyncPin + 1 (DE-9 pin 9). With a
                                   ///< framebuffer, hsyncPin + 2 is also
                                   ///< claimed as an internal handshake
                                   ///< signal - leave that GPIO unwired.
        std::uint8_t videoPin = 0; ///< Shorthand pixel pin assignment:
                                   ///< pixel value bit k on videoPin + k
                                   ///< for every pixelPins entry left at
                                   ///< -1. MDA meaning: bit 0 = VIDEO
                                   ///< (DE-9 pin 7), bit 1 = INTENSITY
                                   ///< (DE-9 pin 6, Packed2 only).
        /// Explicit GPIO per pixel-value bit; entries < 0 fall back to
        /// videoPin + bit. The architecture never assumes consecutive
        /// GPIOs; however, THIS backend's PIO output mapping does
        /// require the active format's pins to be consecutive ascending
        /// GPIOs - an RP2350 implementation detail validated at begin().
        std::array<std::int16_t, kMaxPixelBits> pixelPins{
            -1, -1, -1, -1, -1, -1, -1, -1};
        /// Pixel storage scanned out by the device, or nullptr for
        /// signal-only output (black screen). Any Framebuffer whose
        /// pixelFormat() this backend supports (see
        /// supportsPixelFormat()). Caller-owned; must outlive the
        /// device while active and match the mode's visible size.
        Framebuffer* framebuffer = nullptr;
    };

    /// The configuration is copied; it may be destroyed after construction.
    explicit MdaDisplayDevice(const Config& config);

    bool supports(VideoMode mode) const override;
    bool supportsPixelFormat(PixelFormat format) const override;
    bool begin(VideoMode mode) override;
    void end() override;
    bool isActive() const override;
    std::uint16_t width() const override;
    std::uint16_t height() const override;
    std::uint32_t refreshRateMilliHz() const override;
    DisplaySurface* surface() override;

    /// Replaces the scanned-out framebuffer while output is inactive
    /// (call end() first); the replacement is validated by the next
    /// begin(). Needed to switch between modes with different visible
    /// sizes. Returns false while active.
    bool setFramebuffer(Framebuffer* framebuffer) override;

private:
    /// Timing-table storage, sized for the largest mode this backend
    /// supports. Future MDA-family modes grow this constant, never the
    /// public API.
    static constexpr std::size_t kTimingWords =
        PioScanlineEngine::timingWordsFor(modes::kIbmMda) >
                PioScanlineEngine::timingWordsFor(modes::kMdaOverscan)
            ? PioScanlineEngine::timingWordsFor(modes::kIbmMda)
            : PioScanlineEngine::timingWordsFor(modes::kMdaOverscan);

    std::uint32_t timingTable_[kTimingWords];
    PioScanlineEngine engine_;
    Framebuffer* framebuffer_ = nullptr;
    std::array<std::int16_t, Config::kMaxPixelBits> pixelPins_{}; ///< Resolved per-bit GPIOs.
    const VideoTiming* activeTiming_ = &modes::kIbmMda; ///< Selected mode's timing.
    bool active_ = false;
};

} // namespace picottl::rp2350
