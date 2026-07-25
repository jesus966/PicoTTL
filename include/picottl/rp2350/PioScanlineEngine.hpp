// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// PioScanlineEngine.hpp
// INTERNAL BACKEND COMPONENT - not part of the application API.
// Applications never interact with this class; it is included by backend
// headers out of technical necessity (by-value composition, no heap).
//
// Raster generator of the RP2350 backend, built on PIO + DMA. This is
// the only video-engine component that touches the Pico SDK.
// It owns exactly the hardware resources listed in its Config and claims
// nothing else. The pixel-path resources (second state machine, second
// DMA channel pair, display-enable and video GPIOs) are only claimed
// when a pixel source is attached.
//
// Operation:
//   configure() compiles the VideoTiming into a table of 32-bit timing
//   segments (3 per scanline) inside a caller-provided buffer. Two chained
//   DMA channels then replay that table into the PIO TX FIFO forever:
//   the data channel streams the table, and on completion triggers the
//   control channel, which rewinds the data channel's read address.
//
//   When a pixel source is attached, the timing state machine also drives
//   an internal display-enable GPIO during the visible span of each
//   active line. A second state machine waits on that signal and shifts
//   out exactly hActive one-bit pixels per line, fed by its own pair of
//   chained DMA channels (with byte swap, so the framebuffer is byte-
//   linear, MSB = leftmost pixel).
//
//   The raster and pixel streams therefore run with ZERO CPU involvement.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>

#include "picottl/video/VideoTiming.hpp"

namespace picottl::rp2350 {

/// PIO + DMA raster generator for the RP2350 (internal).
class PioScanlineEngine final {
public:
    /// Hardware resources used by the engine. The engine never selects
    /// resources on its own: everything is chosen by the caller.
    /// Pixel-path resources are ignored (and not claimed) unless a pixel
    /// source is attached with setPixelSource().
    struct Config {
        std::uint8_t pioInstance = 0;       ///< PIO block index (0..2 on RP2350).
        std::uint8_t stateMachine = 0;      ///< Timing state machine (0..3).
        std::uint8_t pixelStateMachine = 1; ///< Pixel state machine (0..3).
        std::uint8_t dmaDataChannel = 0;    ///< DMA channel streaming the timing table.
        std::uint8_t dmaControlChannel = 1; ///< DMA channel that rewinds the data channel.
        std::uint8_t pixelDmaDataChannel = 2;    ///< DMA channel streaming pixel data.
        std::uint8_t pixelDmaControlChannel = 3; ///< DMA channel that rewinds pixel data.
        std::uint8_t hsyncPin = 0;          ///< HSYNC GPIO. VSYNC is hsyncPin + 1; with a
                                            ///< pixel source, hsyncPin + 2 is also claimed
                                            ///< as internal display-enable (leave unwired).
        std::uint8_t videoPin = 0;          ///< Video GPIO (pixel path only).
        std::uint32_t* timingBuffer = nullptr; ///< Caller-provided timing table storage.
        std::size_t timingBufferWords = 0;     ///< Capacity of timingBuffer in 32-bit words.
    };

    /// Timing segments emitted per scanline. The front porch gets its own
    /// segment (when present) so that display-enable covers exactly the
    /// visible pixel window, matching the 6845 CRTC's behaviour.
    static constexpr std::size_t segmentsPerLine(const VideoTiming& timing) {
        return timing.hFrontPorch > 0 ? 4u : 3u;
    }

    /// Number of 32-bit words of timing-buffer storage required for a timing.
    /// Usable in constant expressions to size static buffers.
    static constexpr std::size_t timingWordsFor(const VideoTiming& timing) {
        return static_cast<std::size_t>(timing.vTotal()) * segmentsPerLine(timing);
    }

    explicit PioScanlineEngine(const Config& config);
    ~PioScanlineEngine();

    PioScanlineEngine(const PioScanlineEngine&) = delete;
    PioScanlineEngine& operator=(const PioScanlineEngine&) = delete;

    bool configure(const VideoTiming& timing);
    bool start();
    void stop();
    bool isRunning() const;

    /// Attaches the packed pixel stream scanned out during visible
    /// spans: bitsPerPixel-wide fields (1, 2, 4 or 8 - the depth must
    /// divide 32, the PIO OSR refill granularity), byte-linear,
    /// MSB-first, row-major, exactly hActive*bitsPerPixel/8 bytes per
    /// line and vActive lines. Pixel value bit k drives GPIO
    /// videoPin + k for the wired bits; pinCount (default: the depth)
    /// may be SMALLER than the depth for backends whose wire count
    /// does not divide 32 (e.g. six EGA wires fed by an 8-bit stream:
    /// the surplus high bits are shifted out but never driven). The
    /// electrical meaning of each bit belongs to the display backend.
    /// There is exactly ONE pixel DMA stream regardless of depth (core
    /// architectural invariant); only the PIO program differs.
    /// The data must be 4-byte aligned and its size a multiple of 4.
    /// Must be called before configure(); pass nullptr to detach. The
    /// caller owns the memory, which must stay valid while the engine
    /// runs.
    bool setPixelSource(const std::uint8_t* data, std::size_t sizeBytes,
                        std::uint8_t bitsPerPixel = 1,
                        std::uint8_t pinCount = 0);

private:
    bool buildTimingBuffer(const VideoTiming& timing);
    bool acquireHardware();
    bool acquirePixelHardware();
    bool preparePixelProgram();
    bool applyPioConfig();
    bool applyPixelPioConfig();
    void setupDma();
    void setupPixelDma();
    void driveSignalsIdle();

    Config config_;
    VideoTiming timing_{};
    std::uint32_t timingBufferAddress_ = 0; ///< Source word for the DMA control channel.
    std::size_t wordsUsed_ = 0;
    int programOffset_ = -1;
    bool hardwareAcquired_ = false;
    bool configured_ = false;
    bool running_ = false;

    const std::uint8_t* pixelSource_ = nullptr;
    std::size_t pixelSourceBytes_ = 0;
    std::uint32_t pixelSourceAddress_ = 0; ///< Source word for the pixel control channel.
    std::uint8_t pixelBitsPerPixel_ = 1;
    std::uint8_t pixelPinCount_ = 1; ///< Driven GPIOs (<= stream depth).
    std::uint8_t loadedPixelProgramBpp_ = 0; ///< Depth of the loaded pixel program (0 = none).
    int pixelProgramOffset_ = -1;
    bool pixelHardwareAcquired_ = false;
    float timingClockDivider_ = 0.0f;
};

} // namespace picottl::rp2350
