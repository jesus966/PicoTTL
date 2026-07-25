// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// CgaDisplayDevice.hpp
// IBM CGA display backend for the RP2350.
//
// PUBLIC API - applications construct this device, hand it to
// picottl::Display, and never touch it again. Only the Config struct
// and the DisplayDevice interface are application-facing; everything
// else in this backend is an implementation detail.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>

#include "picottl/DisplayDevice.hpp"
#include "picottl/Framebuffer.hpp"
#include "picottl/rp2350/PioScanlineEngine.hpp" // internal engine (by-value member)
#include "picottl/video/ModeTimings.hpp"        // backend SPI

namespace picottl::rp2350 {

/// IBM CGA-family display backend for the RP2350.
///
/// Serves the CGA rasters (VideoMode::IbmCga640 and IbmCga320) over the
/// RGBI TTL interface of the IBM 5153 and compatibles. Future CGA-family
/// modes (overscan variants) are added inside this backend without
/// public API changes.
///
/// Color semantics: the pixel value is the IBM RGBI color number,
/// verbatim (bit 0 = Blue, bit 1 = Green, bit 2 = Red, bit 3 =
/// Intensity). Color{1} is blue, Color{4} is red, Color{15} is white -
/// every IBM color table applies unchanged. Color{6} (dark yellow)
/// appears as brown on a genuine 5153, which darkens it internally.
///
/// Pixel formats: Packed4 uses all four wires. Narrower formats drive
/// fewer wires and leave the rest at logic low: Packed2 drives Blue and
/// Green, Mono1 drives Blue only (wire that single GPIO to whichever
/// primary you prefer). A backend capability, not an architectural rule.
///
/// Usage:
///   CgaDisplayDevice::Config config;
///   config.hsyncPin = 2;       // GPIO 4 internal handshake, leave unwired
///   config.vsyncPin = 3;
///   config.bluePin = 16;
///   config.greenPin = 17;
///   config.redPin = 18;
///   config.intensityPin = 19;
///   config.framebuffer = &framebuffer;
///   CgaDisplayDevice device(config);
///   picottl::Display display(device);
///   display.begin(picottl::VideoMode::IbmCga640);
///
/// Clock note: a 142.8 MHz system clock gives both CGA modes
/// jitter-free integer clock dividers (-0.27% pixel-clock error) and
/// allows switching between them without reclocking the RP2350. The
/// slightly LOW error is deliberate: the resulting ~59.76 Hz vertical
/// refresh stays capturable by aged monitors whose non-adjustable
/// vertical free-run has drifted (rates above nominal, e.g. from a
/// 144 MHz clock, failed to lock on real hardware). Clock policy
/// belongs to the application.
///
/// RAM note: this object embeds the fixed-size signal table the backend
/// needs (~4.1 KB). The framebuffer memory is provided (and owned) by
/// the application.
class CgaDisplayDevice final : public DisplayDevice {
public:
    /// Hardware resources used by this backend, expressed as the
    /// electrical interface of a CGA monitor: every output signal has
    /// its own named GPIO. The application decides; the framework
    /// validates and configures exactly these resources and claims
    /// nothing else. Whether a particular pin assignment is realizable
    /// is checked at begin(), which returns false for assignments this
    /// platform cannot generate (see begin()).
    struct Config {
        std::uint8_t pioInstance = 0;       ///< PIO block index (0..2).
        std::uint8_t stateMachine = 0;      ///< Signal state machine (0..3).
        std::uint8_t pixelStateMachine = 1; ///< Pixel state machine (0..3).
        std::uint8_t dmaDataChannel = 0;    ///< DMA channel pair for signal
        std::uint8_t dmaControlChannel = 1; ///< generation.
        std::uint8_t pixelDmaDataChannel = 2;    ///< DMA channel pair for
        std::uint8_t pixelDmaControlChannel = 3; ///< pixel streaming.

        std::uint8_t hsyncPin = 0; ///< HSYNC GPIO (DE-9 pin 8). With a
                                   ///< framebuffer, hsyncPin + 2 is also
                                   ///< claimed as an internal handshake
                                   ///< signal - leave that GPIO unwired.
        std::uint8_t vsyncPin = 1; ///< VSYNC GPIO (DE-9 pin 9).

        // RGBI color outputs. Only used when a framebuffer is attached,
        // and only the wires of the active pixel format are claimed.
        // Note the DE-9 carries R,G,B on pins 3,4,5 while the pixel
        // value counts B,G,R upward - three crossed wires in the cable
        // keep Color{n} equal to the IBM color number forever.
        std::uint8_t bluePin = 0;      ///< Blue, pixel value bit 0 (DE-9 pin 5).
        std::uint8_t greenPin = 0;     ///< Green, pixel value bit 1 (DE-9 pin 4).
        std::uint8_t redPin = 0;       ///< Red, pixel value bit 2 (DE-9 pin 3).
        std::uint8_t intensityPin = 0; ///< Intensity, pixel value bit 3 (DE-9 pin 6).

        /// Pixel storage scanned out by the device, or nullptr for
        /// signal-only output (black screen). Any Framebuffer whose
        /// pixelFormat() this backend supports (see
        /// supportsPixelFormat()). Caller-owned; must outlive the
        /// device while active and match the mode's visible size.
        Framebuffer* framebuffer = nullptr;
    };

    /// The configuration is copied; it may be destroyed after construction.
    explicit CgaDisplayDevice(const Config& config);

    bool supports(VideoMode mode) const override;
    bool supportsPixelFormat(PixelFormat format) const override;

    /// Starts the mode. Returns false (leaving the hardware untouched)
    /// for modes outside the CGA family, unsupported pixel formats,
    /// framebuffer size mismatches, and pin assignments this platform
    /// cannot generate. The RP2350's PIO output mapping requires
    /// vsyncPin == hsyncPin + 1 and the active format's color pins to
    /// be consecutive ascending GPIOs in B,G,R,I order - an
    /// implementation detail of this microcontroller, validated here,
    /// never part of the architecture.
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
    /// supports. Future CGA-family modes grow this constant, never the
    /// public API.
    static constexpr std::size_t kTimingWords =
        PioScanlineEngine::timingWordsFor(modes::kIbmCga640) >
                PioScanlineEngine::timingWordsFor(modes::kIbmCga320)
            ? PioScanlineEngine::timingWordsFor(modes::kIbmCga640)
            : PioScanlineEngine::timingWordsFor(modes::kIbmCga320);

    std::uint32_t timingTable_[kTimingWords];
    PioScanlineEngine engine_;
    Config config_;
    Framebuffer* framebuffer_ = nullptr;
    const VideoTiming* activeTiming_ = &modes::kIbmCga640; ///< Selected mode's timing.
    bool active_ = false;
};

} // namespace picottl::rp2350
