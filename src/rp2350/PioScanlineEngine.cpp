// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// PioScanlineEngine.cpp
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "picottl/rp2350/PioScanlineEngine.hpp"
#include "DmaStop.hpp"

#include <cmath>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/pio_instructions.h"

#include "ttl_pixels.pio.h"
#include "ttl_timing.pio.h"

namespace picottl::rp2350 {

namespace {

/// Fixed instruction overhead of one timing segment, in pixel clocks
/// (see ttl_timing.pio). Also the minimum segment length.
constexpr std::uint32_t kSegmentOverhead = 3;

constexpr std::uint32_t kMaxPioClockDivider = 65'536;

bool pioClockDividerValid(float divider) {
    return divider >= 1.0f && divider <= kMaxPioClockDivider;
}

void setPioClockDivider(pio_sm_config* config, float divider) {
    if (divider == kMaxPioClockDivider) {
        // INT=0, FRAC=0 encodes 65536. Avoid the SDK 2.3.0 float
        // helper's out-of-range conversion of 65536 to uint16_t.
        sm_config_set_clkdiv_int_frac8(config, 0, 0);
    } else {
        sm_config_set_clkdiv(config, divider);
    }
}

/// Maximum segment length in pixels (29-bit delay field).
constexpr std::uint32_t kMaxSegmentPixels = ((1u << 29) - 1) + kSegmentOverhead;

/// Packs one timing segment (see ttl_timing.pio):
/// HSYNC/VSYNC/display-enable levels in bits [2:0], delay in bits [31:3].
constexpr std::uint32_t makeSegment(bool hsyncLevel, bool vsyncLevel,
                                    bool displayEnable, std::uint32_t pixels) {
    const std::uint32_t pins = (hsyncLevel ? 1u : 0u) |
                               (vsyncLevel ? 2u : 0u) |
                               (displayEnable ? 4u : 0u);
    const std::uint32_t delay = pixels - kSegmentOverhead;
    return pins | (delay << 3);
}

constexpr bool segmentLengthValid(std::uint32_t pixels) {
    return pixels >= kSegmentOverhead && pixels <= kMaxSegmentPixels;
}

constexpr bool activeLevel(SyncPolarity polarity) {
    return polarity == SyncPolarity::ActiveHigh;
}

/// Pixel program for a given depth. One pixel stream, one state
/// machine - only the program differs per depth (core invariant).
const pio_program_t* pixelProgramFor(std::uint8_t bitsPerPixel) {
    switch (bitsPerPixel) {
        case 2:
            return &ttl_pixels2_program;
        case 4:
            return &ttl_pixels4_program;
        case 8:
            return &ttl_pixels8_program;
        default:
            return &ttl_pixels_program;
    }
}

/// Monitor-facing outputs drive low-impedance TTL summing networks
/// (e.g. the IBM 5151 video amplifier input), often through series
/// protection resistors. A stronger, faster pad lowers the source
/// impedance and keeps edges matched across signals.
void configureMonitorPad(uint pin) {
    gpio_set_drive_strength(pin, GPIO_DRIVE_STRENGTH_8MA);
    gpio_set_slew_rate(pin, GPIO_SLEW_RATE_FAST);
}

} // namespace

PioScanlineEngine::PioScanlineEngine(const Config& config)
    : config_(config) {}

PioScanlineEngine::~PioScanlineEngine() {
    stop();
    if (pixelHardwareAcquired_) {
        PIO pio = pio_get_instance(config_.pioInstance);
        if (pixelProgramOffset_ >= 0) {
            pio_remove_program(pio, pixelProgramFor(loadedPixelProgramBpp_),
                               static_cast<uint>(pixelProgramOffset_));
        }
        pio_sm_unclaim(pio, config_.pixelStateMachine);
        dma_channel_unclaim(config_.pixelDmaDataChannel);
        dma_channel_unclaim(config_.pixelDmaControlChannel);
        pixelHardwareAcquired_ = false;
    }
    if (hardwareAcquired_) {
        PIO pio = pio_get_instance(config_.pioInstance);
        if (programOffset_ >= 0) {
            pio_remove_program(pio, &ttl_timing_program,
                               static_cast<uint>(programOffset_));
        }
        pio_sm_unclaim(pio, config_.stateMachine);
        dma_channel_unclaim(config_.dmaDataChannel);
        dma_channel_unclaim(config_.dmaControlChannel);
        hardwareAcquired_ = false;
    }
}

bool PioScanlineEngine::setPixelSource(const std::uint8_t* data, std::size_t sizeBytes,
                                       std::uint8_t bitsPerPixel,
                                       std::uint8_t pinCount) {
    if (running_) {
        return false;
    }
    if (data == nullptr) {
        pixelSource_ = nullptr;
        pixelSourceBytes_ = 0;
        pixelSourceAddress_ = 0;
        pixelBitsPerPixel_ = 1;
        pixelPinCount_ = 1;
        configured_ = false; // A new configure() is required.
        return true;
    }
    // Depths implemented by this backend. The depth must divide 32:
    // the PIO OSR refills in 32-bit words, so a pixel field may never
    // straddle a word boundary (this is why 6 bpp is impossible here -
    // hardware-diagnosed as a frame-by-frame phase drift). Backends
    // with wire counts that do not divide 32 use the next dividing
    // depth and map fewer output pins (see pinCount).
    if (bitsPerPixel != 1 && bitsPerPixel != 2 && bitsPerPixel != 4 &&
        bitsPerPixel != 8) {
        return false;
    }
    if (pinCount == 0) {
        pinCount = bitsPerPixel;
    }
    if (pinCount > bitsPerPixel) {
        return false;
    }
    // DMA streams the source as 32-bit words.
    if ((reinterpret_cast<std::uintptr_t>(data) & 3u) != 0 ||
        sizeBytes == 0 || (sizeBytes & 3u) != 0) {
        return false;
    }
    pixelSource_ = data;
    pixelSourceBytes_ = sizeBytes;
    pixelSourceAddress_ = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(data));
    pixelBitsPerPixel_ = bitsPerPixel;
    pixelPinCount_ = pinCount;
    configured_ = false; // A new configure() is required.
    return true;
}

bool PioScanlineEngine::configure(const VideoTiming& timing) {
    // Configuration is invalidating, not transactional. Quiesce even an
    // idle configuration: setupDma() has already set EN on its channels.
    // Keep acquisition flags so retries and destruction retain ownership.
    stop();
    configured_ = false;
    const auto reject = [this] {
        // A later stage may have claimed more hardware. SDK claims do
        // not reset registers, so quiesce those resources on every error.
        stop();
        return false;
    };
    if (pixelSource_ != nullptr) {
        // The pixel stream must pack whole bytes per line and cover
        // exactly one frame of visible pixels.
        const std::uint32_t lineBits =
            static_cast<std::uint32_t>(timing.hActive) * pixelBitsPerPixel_;
        if (lineBits % 8u != 0 ||
            pixelSourceBytes_ !=
                static_cast<std::size_t>(lineBits / 8u) * timing.vActive) {
            return reject();
        }
    }
    if (!buildTimingBuffer(timing)) {
        return reject();
    }
    if (!prepareClockConfig(timing) || !hardwareConfigValid()) {
        return reject();
    }
    timing_ = timing;
    if (!hardwareAcquired_) {
        if (!acquireHardware()) {
            return reject();
        }
        hardwareAcquired_ = true;
    }
    if (pixelSource_ != nullptr && !pixelHardwareAcquired_) {
        if (!acquirePixelHardware()) {
            return reject();
        }
        pixelHardwareAcquired_ = true;
    }
    if (pixelSource_ != nullptr && !preparePixelProgram()) {
        return reject();
    }
    applyPioConfig();
    if (pixelSource_ != nullptr) {
        applyPixelPioConfig();
    }
    setupDma();
    if (pixelSource_ != nullptr) {
        setupPixelDma();
    }
    configured_ = true;
    return true;
}

bool PioScanlineEngine::start() {
    if (!configured_) {
        return false;
    }
    if (running_) {
        return true;
    }

    PIO pio = pio_get_instance(config_.pioInstance);
    const bool pixelPath = pixelSource_ != nullptr;

    // Return both state machines to a well-defined program start.
    pio_sm_set_enabled(pio, config_.stateMachine, false);
    pio_sm_clear_fifos(pio, config_.stateMachine);
    pio_sm_restart(pio, config_.stateMachine);
    pio_sm_exec(pio, config_.stateMachine,
                pio_encode_jmp(static_cast<uint>(programOffset_)));
    if (pixelPath) {
        const uint pixelSm = config_.pixelStateMachine;
        pio_sm_set_enabled(pio, pixelSm, false);
        pio_sm_clear_fifos(pio, pixelSm);
        pio_sm_restart(pio, pixelSm);
        pio_sm_exec(pio, pixelSm,
                    pio_encode_jmp(static_cast<uint>(pixelProgramOffset_)));
        // Preload Y = visible pixels per line - 1, then mark the OSR
        // empty so the first OUT autopulls fresh pixel data.
        pio_sm_put(pio, pixelSm, static_cast<std::uint32_t>(timing_.hActive) - 1u);
        pio_sm_exec(pio, pixelSm, pio_encode_pull(false, true));
        pio_sm_exec(pio, pixelSm, pio_encode_mov(pio_y, pio_osr));
        pio_sm_exec(pio, pixelSm, pio_encode_out(pio_null, 32));
    }
    driveSignalsIdle();

    // stop() must clear EN and CHAIN_TO for E5. Rearm both DMA loops
    // while PIO is disabled, including the control channels' EN bits.
    setupDma();
    if (pixelPath) {
        setupPixelDma();
    }

    // Prime the DMA loops. The state machines stall on empty FIFOs, so
    // this ordering is not timing-critical.
    dma_channel_set_read_addr(config_.dmaDataChannel, config_.timingBuffer, false);
    dma_channel_set_trans_count(config_.dmaDataChannel,
                                static_cast<std::uint32_t>(wordsUsed_), true);
    if (pixelPath) {
        dma_channel_set_read_addr(config_.pixelDmaDataChannel, pixelSource_, false);
        dma_channel_set_trans_count(config_.pixelDmaDataChannel,
                                    static_cast<std::uint32_t>(pixelSourceBytes_ / 4u),
                                    true);
        // Start both state machines on the same cycle with their clock
        // dividers phase-aligned.
        pio_enable_sm_mask_in_sync(pio, (1u << config_.stateMachine) |
                                            (1u << config_.pixelStateMachine));
    } else {
        pio_sm_set_enabled(pio, config_.stateMachine, true);
    }

    running_ = true;
    return true;
}

void PioScanlineEngine::stop() {
    if (!hardwareAcquired_) {
        return;
    }

    PIO pio = pio_get_instance(config_.pioInstance);

    // Stop consumption first, but do not assume that this retires DMA
    // transfers: FIFO space and already-issued bus writes can remain.
    pio_sm_set_enabled(pio, config_.stateMachine, false);
    if (pixelHardwareAcquired_) {
        pio_sm_set_enabled(pio, config_.pixelStateMachine, false);
    }

    std::uint32_t mask = (1u << config_.dmaDataChannel) |
                         (1u << config_.dmaControlChannel);
    if (pixelHardwareAcquired_) {
        mask |= (1u << config_.pixelDmaDataChannel) |
                (1u << config_.pixelDmaControlChannel);
    }

    // SDK 2.3.0 dma_channel_abort() does not clear EN. E5 requires all
    // participants disabled before abort, including the control channels
    // writing READ_ADDR_TRIG. Abort the entire group simultaneously.
    detail::stopDmaChannels(mask,
        [](std::uint8_t channel) {
            hw_write_masked(&dma_hw->ch[channel].al1_ctrl,
                            static_cast<std::uint32_t>(channel)
                                << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB,
                            DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS |
                                DMA_CH0_CTRL_TRIG_EN_BITS);
        },
        [](std::uint32_t channels) { dma_hw->abort = channels; },
        [](std::uint32_t channels) {
            if ((dma_hw->abort & channels) != 0) {
                return true;
            }
            // Also retain the SDK's BUSY retirement check before touching
            // FIFOs, reconfiguring or releasing either member of a pair.
            for (uint channel = 0; channel < NUM_DMA_CHANNELS; ++channel) {
                if ((channels & (1u << channel)) != 0 &&
                    dma_channel_is_busy(channel)) {
                    return true;
                }
            }
            return false;
        },
        [] { tight_loop_contents(); });
    // The engine never enables DMA IRQs; discard completion status that
    // may have been latched during abort before these channels are reused.
    dma_hw->intr = mask;

    // Clear even idle/partially configured owned resources. Only execute
    // the idle pin instruction when a successful start initialized PIO.
    pio_sm_clear_fifos(pio, config_.stateMachine);
    if (pixelHardwareAcquired_) {
        pio_sm_clear_fifos(pio, config_.pixelStateMachine);
    }
    if (running_) {
        driveSignalsIdle();
    }

    running_ = false;
}

bool PioScanlineEngine::isRunning() const {
    return running_;
}

bool PioScanlineEngine::buildTimingBuffer(const VideoTiming& timing) {
    const std::size_t required = timingWordsFor(timing);
    if (required == 0 || config_.timingBuffer == nullptr || config_.timingBufferWords < required) {
        return false;
    }

    const std::uint32_t activeSpan = timing.hActive;
    const std::uint32_t frontSpan = timing.hFrontPorch;
    const std::uint32_t syncSpan = timing.hSyncWidth;
    const std::uint32_t backSpan = timing.hBackPorch;
    const bool splitFrontPorch = frontSpan > 0;

    if (!segmentLengthValid(activeSpan) || !segmentLengthValid(syncSpan) ||
        !segmentLengthValid(backSpan) ||
        (splitFrontPorch && !segmentLengthValid(frontSpan))) {
        return false;
    }

    const bool hsActive = activeLevel(timing.hSyncPolarity);
    const bool vsActive = activeLevel(timing.vSyncPolarity);
    const bool hsIdle = !hsActive;
    const bool vsIdle = !vsActive;

    std::uint32_t* out = config_.timingBuffer;
    const auto emitLine = [&](bool vsyncLevel, bool displayEnable) {
        // Display-enable marks exactly the visible pixel window, matching
        // the 6845 CRTC's Display Enable behaviour.
        *out++ = makeSegment(hsIdle, vsyncLevel, displayEnable, activeSpan);
        if (splitFrontPorch) {
            *out++ = makeSegment(hsIdle, vsyncLevel, false, frontSpan);
        }
        *out++ = makeSegment(hsActive, vsyncLevel, false, syncSpan);
        *out++ = makeSegment(hsIdle, vsyncLevel, false, backSpan);
    };

    for (std::uint32_t i = 0; i < timing.vActive; ++i) {
        emitLine(vsIdle, true);
    }
    for (std::uint32_t i = 0; i < timing.vFrontPorch; ++i) {
        emitLine(vsIdle, false);
    }
    for (std::uint32_t i = 0; i < timing.vSyncWidth; ++i) {
        emitLine(vsActive, false);
    }
    for (std::uint32_t i = 0; i < timing.vBackPorch; ++i) {
        emitLine(vsIdle, false);
    }

    wordsUsed_ = required;
    timingBufferAddress_ = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(config_.timingBuffer));
    return true;
}

bool PioScanlineEngine::hardwareConfigValid() const {
    if (config_.pioInstance >= NUM_PIOS ||
        config_.stateMachine >= NUM_PIO_STATE_MACHINES ||
        config_.dmaDataChannel >= NUM_DMA_CHANNELS ||
        config_.dmaControlChannel >= NUM_DMA_CHANNELS ||
        config_.dmaDataChannel == config_.dmaControlChannel ||
        config_.hsyncPin + 1 >= NUM_BANK0_GPIOS) {
        return false;
    }

    if (pixelSource_ == nullptr) {
        return true;
    }
    const std::uint32_t displayEnablePin = config_.hsyncPin + 2u;

    if (config_.pixelStateMachine >= NUM_PIO_STATE_MACHINES ||
        config_.pixelStateMachine == config_.stateMachine ||
        config_.pixelDmaDataChannel >= NUM_DMA_CHANNELS ||
        config_.pixelDmaControlChannel >= NUM_DMA_CHANNELS ||
        config_.pixelDmaDataChannel == config_.pixelDmaControlChannel ||
        config_.pixelDmaDataChannel == config_.dmaDataChannel ||
        config_.pixelDmaDataChannel == config_.dmaControlChannel ||
        config_.pixelDmaControlChannel == config_.dmaDataChannel ||
        config_.pixelDmaControlChannel == config_.dmaControlChannel ||
        displayEnablePin >= NUM_BANK0_GPIOS) {
        return false;
    }

    const std::uint32_t videoLastPin = config_.videoPin + pixelPinCount_ - 1u;
    // The video pin span must exist and not overlap the sync/handshake
    // pins.
    if (videoLastPin >= NUM_BANK0_GPIOS ||
        (videoLastPin >= config_.hsyncPin &&
         config_.videoPin <= displayEnablePin)) {
        return false;
    }

    return true;
}

bool PioScanlineEngine::acquireHardware() {
    PIO pio = pio_get_instance(config_.pioInstance);

    // Fail gracefully instead of hitting the SDK's claim asserts.
    if (pio_sm_is_claimed(pio, config_.stateMachine) ||
        dma_channel_is_claimed(config_.dmaDataChannel) ||
        dma_channel_is_claimed(config_.dmaControlChannel) ||
        !pio_can_add_program(pio, &ttl_timing_program)) {
        return false;
    }

    programOffset_ = pio_add_program(pio, &ttl_timing_program);
    if (programOffset_ < 0) {
        return false;
    }
    pio_sm_claim(pio, config_.stateMachine);
    dma_channel_claim(config_.dmaDataChannel);
    dma_channel_claim(config_.dmaControlChannel);
    return true;
}

bool PioScanlineEngine::acquirePixelHardware() {
    PIO pio = pio_get_instance(config_.pioInstance);

    if (pio_sm_is_claimed(pio, config_.pixelStateMachine) ||
        dma_channel_is_claimed(config_.pixelDmaDataChannel) ||
        dma_channel_is_claimed(config_.pixelDmaControlChannel)) {
        return false;
    }

    pio_sm_claim(pio, config_.pixelStateMachine);
    dma_channel_claim(config_.pixelDmaDataChannel);
    dma_channel_claim(config_.pixelDmaControlChannel);

    return true;
}

bool PioScanlineEngine::preparePixelProgram() {
    PIO pio = pio_get_instance(config_.pioInstance);
    const std::uint8_t bpp = pixelBitsPerPixel_;
    const std::uint32_t videoLastPin = config_.videoPin + pixelPinCount_ - 1u;
    if (loadedPixelProgramBpp_ != bpp) {
        if (pixelProgramOffset_ >= 0) {
            pio_remove_program(pio, pixelProgramFor(loadedPixelProgramBpp_),
                               static_cast<uint>(pixelProgramOffset_));
            pixelProgramOffset_ = -1;
            loadedPixelProgramBpp_ = 0; // No program remains if the next allocation fails.
        }
        const pio_program_t* program = pixelProgramFor(bpp);
        if (!pio_can_add_program(pio, program)) {
            return false;
        }
        pixelProgramOffset_ = pio_add_program(pio, program);
        if (pixelProgramOffset_ < 0) {
            return false;
        }
        loadedPixelProgramBpp_ = bpp;
    }

    // Pixel value bit k drives GPIO videoPin + k, for the wired bits
    // (pinCount may be smaller than the stream depth: surplus high
    // bits are shifted but not driven).
    for (std::uint32_t pin = config_.videoPin; pin <= videoLastPin; ++pin) {
        pio_gpio_init(pio, pin);
        configureMonitorPad(pin);
    }
    pio_sm_set_consecutive_pindirs(pio, config_.pixelStateMachine,
                                   config_.videoPin, pixelPinCount_, true);
    return true;
}

bool PioScanlineEngine::prepareClockConfig(const VideoTiming& timing) {
    const std::uint32_t systemHz = clock_get_hz(clk_sys);
    const std::uint32_t pixelHz = timing.pixelClockHz;
    // Check the exact ratio before float rounding can hide an invalid
    // endpoint. The widened product fits for every uint32_t frequency.
    if (pixelHz == 0 || systemHz < pixelHz ||
        static_cast<std::uint64_t>(systemHz) >
            static_cast<std::uint64_t>(pixelHz) * kMaxPioClockDivider) {
        return false;
    }
    const float systemClockHz = static_cast<float>(systemHz);
    const float pixelClockHz = static_cast<float>(pixelHz);
    float divider = systemClockHz / pixelClockHz;

    // Prefer a jitter-free integer divider when the resulting pixel clock
    // stays within 1% of nominal (CRT monitors tolerate far more).
    const float rounded = std::round(divider);
    if (rounded >= 1.0f) {
        const float error =
            std::fabs(systemClockHz / rounded - pixelClockHz) / pixelClockHz;
        if (error <= 0.01f) {
            divider = rounded;
        }
    }
    // Validate the pixel path after snapping: e.g. 1.98 can legitimately
    // snap to 2, giving a pixel state machine divider of exactly 1.
    if (!pioClockDividerValid(divider) ||
        (pixelSource_ != nullptr && !pioClockDividerValid(divider / 2.0f))) {
        return false;
    }
    timingClockDivider_ = divider;
    return true;
}

void PioScanlineEngine::applyPioConfig() {
    PIO pio = pio_get_instance(config_.pioInstance);

    // Re-own the sync (and, on the pixel path, display-enable) GPIOs on
    // EVERY configure, not only at first acquire: in multi-backend
    // applications another device on a different PIO block may have
    // re-muxed these pins since we last ran. Without this, a device
    // resumed after another backend generates its raster into a PIO
    // whose pins no longer listen to it (no sync output at all). The
    // pixel pins already get the same treatment in preparePixelProgram().
    pio_gpio_init(pio, config_.hsyncPin);
    pio_gpio_init(pio, config_.hsyncPin + 1u);
    configureMonitorPad(config_.hsyncPin);
    configureMonitorPad(config_.hsyncPin + 1u);
    if (pixelSource_ != nullptr) {
        pio_gpio_init(pio, config_.hsyncPin + 2u); // display-enable
    }
    pio_sm_set_consecutive_pindirs(pio, config_.stateMachine,
                                   config_.hsyncPin,
                                   pixelSource_ != nullptr ? 3 : 2, true);

    pio_sm_config c = ttl_timing_program_get_default_config(
        static_cast<uint>(programOffset_));
    // The word format always carries 3 signal bits; the display-enable
    // pin is only mapped (and claimed) when the pixel path is in use.
    sm_config_set_out_pins(&c, config_.hsyncPin,
                           pixelSource_ != nullptr ? 3 : 2);
    sm_config_set_out_shift(&c, /*shift_right=*/true, /*autopull=*/true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    setPioClockDivider(&c, timingClockDivider_);

    pio_sm_init(pio, config_.stateMachine,
                static_cast<uint>(programOffset_), &c);
    driveSignalsIdle();
}

void PioScanlineEngine::applyPixelPioConfig() {
    PIO pio = pio_get_instance(config_.pioInstance);

    // The pixel program takes 2 cycles per pixel (out + jmp), so its
    // state machine runs at twice the pixel clock.
    const float divider = timingClockDivider_ / 2.0f;

    pio_sm_config c =
        pixelBitsPerPixel_ == 8
            ? ttl_pixels8_program_get_default_config(
                  static_cast<uint>(pixelProgramOffset_))
            : (pixelBitsPerPixel_ == 4
                   ? ttl_pixels4_program_get_default_config(
                         static_cast<uint>(pixelProgramOffset_))
                   : (pixelBitsPerPixel_ == 2
                          ? ttl_pixels2_program_get_default_config(
                                static_cast<uint>(pixelProgramOffset_))
                          : ttl_pixels_program_get_default_config(
                                static_cast<uint>(pixelProgramOffset_))));
    sm_config_set_in_pins(&c, config_.hsyncPin + 2u); // display-enable
    sm_config_set_out_pins(&c, config_.videoPin, pixelPinCount_);
    // Shift left: bit 31 first. Combined with DMA byte swap this makes
    // the framebuffer byte-linear with MSB = leftmost pixel.
    sm_config_set_out_shift(&c, /*shift_right=*/false, /*autopull=*/true, 32);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    setPioClockDivider(&c, divider);

    pio_sm_init(pio, config_.pixelStateMachine,
                static_cast<uint>(pixelProgramOffset_), &c);
}

void PioScanlineEngine::setupDma() {
    PIO pio = pio_get_instance(config_.pioInstance);

    // Data channel: timing table -> PIO TX FIFO, then trigger the control
    // channel.
    dma_channel_config dataConfig =
        dma_channel_get_default_config(config_.dmaDataChannel);
    channel_config_set_transfer_data_size(&dataConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&dataConfig, true);
    channel_config_set_write_increment(&dataConfig, false);
    channel_config_set_dreq(&dataConfig,
                            pio_get_dreq(pio, config_.stateMachine, true));
    channel_config_set_chain_to(&dataConfig, config_.dmaControlChannel);
    dma_channel_configure(config_.dmaDataChannel, &dataConfig,
                          &pio->txf[config_.stateMachine],
                          config_.timingBuffer,
                          static_cast<std::uint32_t>(wordsUsed_),
                          false);

    // Control channel: rewrites the data channel's read address, which
    // also retriggers it. This closes the loop with zero CPU involvement.
    dma_channel_config controlConfig =
        dma_channel_get_default_config(config_.dmaControlChannel);
    channel_config_set_transfer_data_size(&controlConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&controlConfig, false);
    channel_config_set_write_increment(&controlConfig, false);
    dma_channel_configure(config_.dmaControlChannel, &controlConfig,
                          &dma_hw->ch[config_.dmaDataChannel].al3_read_addr_trig,
                          &timingBufferAddress_,
                          1,
                          false);
}

void PioScanlineEngine::setupPixelDma() {
    PIO pio = pio_get_instance(config_.pioInstance);

    // Data channel: framebuffer -> pixel SM TX FIFO, one frame per pass.
    dma_channel_config dataConfig =
        dma_channel_get_default_config(config_.pixelDmaDataChannel);
    channel_config_set_transfer_data_size(&dataConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&dataConfig, true);
    channel_config_set_write_increment(&dataConfig, false);
    // Byte swap so the lowest-addressed framebuffer byte is displayed
    // first (the pixel SM shifts MSB first).
    channel_config_set_bswap(&dataConfig, true);
    channel_config_set_dreq(&dataConfig,
                            pio_get_dreq(pio, config_.pixelStateMachine, true));
    channel_config_set_chain_to(&dataConfig, config_.pixelDmaControlChannel);
    dma_channel_configure(config_.pixelDmaDataChannel, &dataConfig,
                          &pio->txf[config_.pixelStateMachine],
                          pixelSource_,
                          static_cast<std::uint32_t>(pixelSourceBytes_ / 4u),
                          false);

    // Control channel: rewinds the data channel to the framebuffer start.
    dma_channel_config controlConfig =
        dma_channel_get_default_config(config_.pixelDmaControlChannel);
    channel_config_set_transfer_data_size(&controlConfig, DMA_SIZE_32);
    channel_config_set_read_increment(&controlConfig, false);
    channel_config_set_write_increment(&controlConfig, false);
    dma_channel_configure(config_.pixelDmaControlChannel, &controlConfig,
                          &dma_hw->ch[config_.pixelDmaDataChannel].al3_read_addr_trig,
                          &pixelSourceAddress_,
                          1,
                          false);
}

void PioScanlineEngine::driveSignalsIdle() {
    PIO pio = pio_get_instance(config_.pioInstance);
    const std::uint32_t hsIdle =
        activeLevel(timing_.hSyncPolarity) ? 0u : 1u;
    const std::uint32_t vsIdle =
        activeLevel(timing_.vSyncPolarity) ? 0u : 1u;
    const std::uint32_t values =
        (hsIdle << config_.hsyncPin) | (vsIdle << (config_.hsyncPin + 1u));
    std::uint32_t mask = 3u << config_.hsyncPin;
    if (pixelHardwareAcquired_) {
        mask |= 1u << (config_.hsyncPin + 2u); // display-enable low
        // All pixel output pins low.
        mask |= ((1u << pixelPinCount_) - 1u) << config_.videoPin;
    }
    pio_sm_set_pins_with_mask(pio, config_.stateMachine, values, mask);
}

} // namespace picottl::rp2350
