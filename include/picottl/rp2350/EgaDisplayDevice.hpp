// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// EgaDisplayDevice.hpp
// IBM EGA display backend for the RP2350.
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

/// IBM EGA-family display backend for the RP2350.
///
/// Serves the enhanced 350-line raster (VideoMode::IbmEga350) of the
/// IBM 5154 Enhanced Color Display over the six-wire rgbRGB TTL
/// interface. Because IBM EGA is hardware-COMPATIBLE with CGA timings
/// (the card scans at 15.7 kHz for 200-line modes and 21.85 kHz for
/// 350-line modes; the 5154 selects by VSYNC polarity), this backend
/// also supports VideoMode::IbmCga640 and IbmCga320 - it references
/// the existing timing catalog entries directly. Compatibility, not
/// duplication.
///
/// Color semantics: the pixel value is the IBM EGA 6-bit color value,
/// verbatim, in IBM's own "internal card bit order" rgbRGB:
///   bit 0 = Primary Blue    bit 3 = Secondary Blue
///   bit 1 = Primary Green   bit 4 = Secondary Green
///   bit 2 = Primary Red     bit 5 = Secondary Red
/// Primaries carry 2/3 amplitude, secondaries 1/3 - four levels per
/// gun, 64 colors. Color{7} is white, Color{0x14} is brown, Color{0x3F}
/// is bright white; every IBM EGA color table applies unchanged.
///
/// Pixel formats: Packed8 is the EGA scanout format on this platform,
/// storing one pixel per byte with the 6-bit EGA color value in the
/// low six bits (bits 6-7 are never driven; keep values in 0..63).
/// This is an RP2350 IMPLEMENTATION LIMITATION, not a framework rule:
/// the PIO shift register refills in 32-bit words, so the scanout
/// stream depth must divide 32 - a byte-linear 6-bit stream cannot be
/// shifted out continuously (Packed6 remains a valid portable memory
/// format; the host backend scans it, this backend cannot). Narrower
/// formats drive fewer wires and leave the rest at logic low: Packed4
/// drives B,G,R,b - note that bit 3 is Secondary BLUE, NOT the CGA
/// intensity; in 200-line modes a standard 5154 interprets the
/// connector as CGA (intensity = Secondary Green = bit 4, pins 2/7
/// ignored), so CGA-compatible 200-line content should use Packed8
/// with the kCga200Palette indices from picottl/ega/Colors.hpp.
///
/// Usage:
///   EgaDisplayDevice::Config config;
///   config.hsyncPin = 2;             // GPIO 4 internal, leave unwired
///   config.vsyncPin = 3;
///   config.primaryBluePin = 16;
///   config.primaryGreenPin = 17;
///   config.primaryRedPin = 18;
///   config.secondaryBluePin = 19;
///   config.secondaryGreenPin = 20;
///   config.secondaryRedPin = 21;
///   config.framebuffer = &framebuffer;
///   EgaDisplayDevice device(config);
///   picottl::Display display(device);
///   display.begin(picottl::VideoMode::IbmEga350);
///
/// Clock note: the 350-line raster shares MDA's 16.257 MHz pixel
/// crystal, so a 130 MHz system clock is ideal (integer divider 8,
/// -0.043% error, ~60.01 Hz vertical). The 200-line CGA-compatible
/// modes belong to the 14.318 MHz crystal domain (142.8 MHz system
/// clock recommended); no single system clock serves both domains
/// with jitter-free dividers, so switching between the families means
/// the application reclocks between end() and begin(). Clock policy
/// belongs to the application.
///
/// RAM note: this object embeds the fixed-size signal table the
/// backend needs (~5.9 KB). Framebuffer memory is provided (and owned)
/// by the application; a Packed8 640x350 framebuffer is 224,000 bytes.
class EgaDisplayDevice final : public DisplayDevice {
public:
    /// Hardware resources used by this backend, expressed as the
    /// electrical interface of an EGA monitor using IBM's own signal
    /// terminology (DE-9: 1 = GND, 2 = Secondary Red, 3 = Primary Red,
    /// 4 = Primary Green, 5 = Primary Blue, 6 = Secondary Green /
    /// Intensity, 7 = Secondary Blue / Mono Video, 8 = HSYNC,
    /// 9 = VSYNC). The application decides; the framework validates
    /// and configures exactly these resources and claims nothing else.
    /// Whether a pin assignment is realizable is checked at begin(),
    /// which returns false for assignments this platform cannot
    /// generate (see begin()).
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

        // rgbRGB color outputs (IBM terminology). Only used when a
        // framebuffer is attached, and only the wires of the active
        // pixel format are claimed. Note the crossed wiring: the DE-9
        // carries r,R,G,B,g,b on pins 2,3,4,5,6,7 while the pixel
        // value counts B,G,R,b,g,r upward - crossed once in the cable,
        // Color{n} equals the IBM EGA color value forever.
        std::uint8_t primaryRedPin = 0;     ///< Primary Red, bit 2 (DE-9 pin 3).
        std::uint8_t primaryGreenPin = 0;   ///< Primary Green, bit 1 (DE-9 pin 4).
        std::uint8_t primaryBluePin = 0;    ///< Primary Blue, bit 0 (DE-9 pin 5).
        std::uint8_t secondaryRedPin = 0;   ///< Secondary Red, bit 5 (DE-9 pin 2).
        std::uint8_t secondaryGreenPin = 0; ///< Secondary Green / Intensity,
                                            ///< bit 4 (DE-9 pin 6).
        std::uint8_t secondaryBluePin = 0;  ///< Secondary Blue / Mono Video,
                                            ///< bit 3 (DE-9 pin 7).

        /// Pixel storage scanned out by the device, or nullptr for
        /// signal-only output (black screen). Any Framebuffer whose
        /// pixelFormat() this backend supports (see
        /// supportsPixelFormat()). Caller-owned; must outlive the
        /// device while active and match the mode's visible size.
        Framebuffer* framebuffer = nullptr;
    };

    /// The configuration is copied; it may be destroyed after construction.
    explicit EgaDisplayDevice(const Config& config);

    bool supports(VideoMode mode) const override;
    bool supportsPixelFormat(PixelFormat format) const override;

    /// Starts the mode. Returns false (leaving the hardware untouched)
    /// for modes outside the EGA family's reach, unsupported pixel
    /// formats, framebuffer size mismatches, and pin assignments this
    /// platform cannot generate. The RP2350's PIO output mapping
    /// requires vsyncPin == hsyncPin + 1 and the active format's color
    /// pins to be consecutive ascending GPIOs in rgbRGB bit order from
    /// primaryBluePin upward - an implementation detail of this
    /// microcontroller, validated here, never part of the architecture.
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
    /// Largest timing table over the supported modes (the 350-line
    /// raster; the referenced CGA entries are smaller). Future
    /// EGA-family modes grow this constant, never the public API.
    static constexpr std::size_t kTimingWords =
        PioScanlineEngine::timingWordsFor(modes::kIbmEga350) >
                (PioScanlineEngine::timingWordsFor(modes::kIbmCga640) >
                         PioScanlineEngine::timingWordsFor(modes::kIbmCga320)
                     ? PioScanlineEngine::timingWordsFor(modes::kIbmCga640)
                     : PioScanlineEngine::timingWordsFor(modes::kIbmCga320))
            ? PioScanlineEngine::timingWordsFor(modes::kIbmEga350)
            : (PioScanlineEngine::timingWordsFor(modes::kIbmCga640) >
                       PioScanlineEngine::timingWordsFor(modes::kIbmCga320)
                   ? PioScanlineEngine::timingWordsFor(modes::kIbmCga640)
                   : PioScanlineEngine::timingWordsFor(modes::kIbmCga320));

    std::uint32_t timingTable_[kTimingWords];
    PioScanlineEngine engine_;
    Config config_;
    Framebuffer* framebuffer_ = nullptr;
    const VideoTiming* activeTiming_ = &modes::kIbmEga350; ///< Selected mode's timing.
    bool active_ = false;
};

} // namespace picottl::rp2350
