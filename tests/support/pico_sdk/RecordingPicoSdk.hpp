// SPDX-License-Identifier: MIT
#pragma once

#include <array>
#include <cassert>
#include <cstdint>

// A recording boundary for the SDK calls used by PioScanlineEngine.
// No PIO instructions, DMA transfers, DREQ timing or bus races are executed.
using uint = unsigned;
constexpr int NUM_PIOS = 3, NUM_PIO_STATE_MACHINES = 4;
constexpr int NUM_DMA_CHANNELS = 16;
#ifndef PICOTTL_TEST_GPIO_COUNT
#define PICOTTL_TEST_GPIO_COUNT 30
#endif
constexpr int NUM_BANK0_GPIOS = PICOTTL_TEST_GPIO_COUNT;
constexpr std::uint32_t DMA_CH0_CTRL_TRIG_EN_BITS = 1;
constexpr std::uint32_t DMA_CH0_CTRL_TRIG_BUSY_BITS = 0x04000000;
constexpr std::uint32_t DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS = 0x0001e000;
constexpr uint DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB = 13;
constexpr uint clk_sys = 0, GPIO_DRIVE_STRENGTH_8MA = 8;
constexpr uint GPIO_SLEW_RATE_FAST = 1, DMA_SIZE_32 = 2, PIO_FIFO_JOIN_TX = 1;
constexpr uint pio_y = 1, pio_osr = 2, pio_null = 3;

struct pio_program_t { uint id; };
struct pio_sm_config {
    float divider = 1;
    std::uint32_t clkdiv = 0x00010000;
    bool fullPeriodEncoded = false;
    int outPin = -1, inPin = -1;
    uint outCount = 0;
};
struct dma_channel_config { std::uint32_t ctrl = 1; };
struct PioRegisters { std::uint32_t txf[4]{}; unsigned sdkCalls = 0; };
using PIO = PioRegisters*;

namespace picottl_test::sdk {
struct DmaCall {
    const volatile void* read = nullptr;
    volatile void* write = nullptr;
    std::uint32_t count = 0;
    unsigned configurations = 0;
};
struct Recording {
    std::uint32_t clockHz = 130'000'000;
    bool roundDividerNearest = true; // SDK 2.3.0 default; false models its truncation option.
    std::array<bool, 16> dmaClaimed{};
    std::array<std::array<bool, 4>, 3> smClaimed{}, smEnabled{};
    std::array<std::array<bool, 5>, 3> programs{};
    std::array<DmaCall, 16> dma{};
    bool allowTimingProgram = true, allowPixelProgram = true;
    bool failTimingProgramAdd = false, failPixelProgramAdd = false;
    unsigned pinDirections = 0, pinWrites = 0, padConfigurations = 0;
    unsigned calls = 0, triggers = 0, aborts = 0, releases = 0;
    unsigned invalidPc = 0, abortWhileEnabled = 0;
    unsigned clockReads = 0, dividerCalls = 0, integerDividerCalls = 0;
    unsigned invalidDividerCalls = 0, floatEndpointCalls = 0, gpioConfigurations = 0;
    std::array<std::array<unsigned, 4>, 3> pioConfigurations{};
    std::array<std::array<float, 4>, 3> dividers{};
    std::array<std::array<std::uint32_t, 4>, 3> clkdivRegisters{};
    std::array<std::array<bool, 4>, 3> fullPeriodEncoded{};
    std::array<uint, 3> gpioBase{};
    std::array<int, 48> gpioFunction{};
    std::array<std::array<int, 4>, 3> outRelative{}, inRelative{};
    std::array<std::uint32_t, 3> outputMask{}, outputValues{}, directionMask{};
    unsigned invalidPinCalls = 0;
    std::uint32_t lastAbort = 0;
};
inline Recording recording;
inline std::array<PioRegisters, 3> pios;
inline uint index(const PioRegisters* pio) { return static_cast<uint>(pio - pios.data()); }
} // namespace picottl_test::sdk

struct DmaRegisters {
    std::uint32_t ctrl_trig = 0;
    // Both spellings access the same recorded configuration; no MMIO layout.
    std::uint32_t& al1_ctrl = ctrl_trig;
    std::uint32_t al3_read_addr_trig = 0;
};
struct AbortRegister {
    void operator=(std::uint32_t mask);
    // Abort is completed immediately at this boundary, not simulated.
    operator std::uint32_t() const { return 0; }
};
struct DmaHardware {
    DmaRegisters ch[16];
    AbortRegister abort;
    std::uint32_t intr = 0;
};
inline DmaHardware recordedDma;
inline DmaHardware* dma_hw = &recordedDma;
inline void AbortRegister::operator=(std::uint32_t mask) {
    auto& r = picottl_test::sdk::recording;
    ++r.calls; ++r.aborts; r.lastAbort = mask;
    for (uint ch = 0; ch < NUM_DMA_CHANNELS; ++ch) {
        if ((mask & (1u << ch)) != 0) {
            if ((dma_hw->ch[ch].ctrl_trig & DMA_CH0_CTRL_TRIG_EN_BITS) != 0) {
                ++r.abortWhileEnabled;
            }
            dma_hw->ch[ch].ctrl_trig &= ~DMA_CH0_CTRL_TRIG_BUSY_BITS;
        }
    }
}

namespace picottl_test::sdk {
inline void reset() {
    recording = {};
    pios = {};
    for (auto& ch : dma_hw->ch) {
        ch.ctrl_trig = 0;
        ch.al3_read_addr_trig = 0;
    }
    dma_hw->intr = 0;
}
} // namespace picottl_test::sdk

inline std::uint32_t clock_get_hz(uint) { auto& r = picottl_test::sdk::recording; ++r.calls; ++r.clockReads; return r.clockHz; }
inline PIO pio_get_instance(uint n) { assert(n < NUM_PIOS); ++picottl_test::sdk::recording.calls; return &picottl_test::sdk::pios[n]; }
inline uint pio_get_gpio_base(const PioRegisters* p) { return picottl_test::sdk::recording.gpioBase[picottl_test::sdk::index(p)]; }
inline bool pio_sm_is_claimed(PIO p, uint sm) { ++p->sdkCalls; return picottl_test::sdk::recording.smClaimed[picottl_test::sdk::index(p)][sm]; }
inline void pio_sm_claim(PIO p, uint sm) { ++p->sdkCalls; auto& r = picottl_test::sdk::recording; assert(!pio_sm_is_claimed(p, sm)); r.smClaimed[picottl_test::sdk::index(p)][sm] = true; ++r.calls; }
inline void pio_sm_unclaim(PIO p, uint sm) { ++p->sdkCalls; auto& r = picottl_test::sdk::recording; assert(pio_sm_is_claimed(p, sm)); r.smClaimed[picottl_test::sdk::index(p)][sm] = false; ++r.calls; ++r.releases; }
inline void pio_sm_set_enabled(PIO p, uint sm, bool enabled) { ++p->sdkCalls; auto& r = picottl_test::sdk::recording; r.smEnabled[picottl_test::sdk::index(p)][sm] = enabled; ++r.calls; }
inline void pio_enable_sm_mask_in_sync(PIO p, uint mask) { ++p->sdkCalls; for (uint sm = 0; sm < 4; ++sm) if ((mask & (1u << sm)) != 0) pio_sm_set_enabled(p, sm, true); }
inline bool pio_can_add_program(PIO, const pio_program_t* program) { return program->id == 0 ? picottl_test::sdk::recording.allowTimingProgram : picottl_test::sdk::recording.allowPixelProgram; }
inline int pio_add_program(PIO p, const pio_program_t* program) { ++p->sdkCalls; auto& r = picottl_test::sdk::recording; assert(pio_can_add_program(p, program)); if (program->id == 0 ? r.failTimingProgramAdd : r.failPixelProgramAdd) return -1; assert(!r.programs[picottl_test::sdk::index(p)][program->id]); r.programs[picottl_test::sdk::index(p)][program->id] = true; ++r.calls; return static_cast<int>(program->id * 4); }
inline void pio_remove_program(PIO p, const pio_program_t* program, uint offset) { ++p->sdkCalls; auto& r = picottl_test::sdk::recording; assert(offset == program->id * 4); assert(r.programs[picottl_test::sdk::index(p)][program->id]); r.programs[picottl_test::sdk::index(p)][program->id] = false; ++r.calls; }
inline void pio_sm_init(PIO p, uint sm, uint pc, const pio_sm_config* c) {
    ++p->sdkCalls;
    auto& r = picottl_test::sdk::recording;
    const auto instance = picottl_test::sdk::index(p);
    const uint base = pio_get_gpio_base(p);
    if ((c->outPin >= 0 && (static_cast<uint>(c->outPin) < base ||
            static_cast<uint>(c->outPin) + c->outCount > base + 32)) ||
        (c->inPin >= 0 && (static_cast<uint>(c->inPin) < base ||
            static_cast<uint>(c->inPin) >= base + 32))) {
        ++r.invalidPinCalls;
    }
    r.outRelative[instance][sm] = c->outPin < 0 ? -1 : c->outPin - static_cast<int>(base);
    r.inRelative[instance][sm] = c->inPin < 0 ? -1 : c->inPin - static_cast<int>(base);
    if (pc >= 32) ++r.invalidPc;
    ++r.pioConfigurations[instance][sm];
    r.dividers[instance][sm] = c->divider;
    r.clkdivRegisters[instance][sm] = c->clkdiv;
    r.fullPeriodEncoded[instance][sm] = c->fullPeriodEncoded;
    pio_sm_set_enabled(p, sm, false);
}
inline uint pio_get_dreq(PIO, uint sm, bool) { return sm; }
inline void pio_sm_clear_fifos(PIO, uint) { ++picottl_test::sdk::recording.calls; }
inline void pio_sm_restart(PIO, uint) { ++picottl_test::sdk::recording.calls; }
inline void pio_sm_exec(PIO, uint, uint) { ++picottl_test::sdk::recording.calls; }
inline void pio_sm_put(PIO, uint, std::uint32_t) { ++picottl_test::sdk::recording.calls; }
inline void pio_sm_set_pins_with_mask64(PIO p, uint, std::uint64_t values, std::uint64_t mask) {
    auto& r = picottl_test::sdk::recording;
    const uint instance = picottl_test::sdk::index(p), base = pio_get_gpio_base(p);
    if ((mask & ~(UINT64_C(0xffffffff) << base)) != 0) ++r.invalidPinCalls;
    const auto relativeMask = static_cast<std::uint32_t>(mask >> base);
    r.outputMask[instance] = relativeMask;
    r.outputValues[instance] = (r.outputValues[instance] & ~relativeMask) |
        (static_cast<std::uint32_t>(values >> base) & relativeMask);
    ++r.calls; ++r.pinWrites;
}
inline void pio_sm_set_pins_with_mask(PIO p, uint sm, std::uint32_t values, std::uint32_t mask) {
    pio_sm_set_pins_with_mask64(p, sm, values, mask);
}
inline void pio_sm_set_consecutive_pindirs(PIO p, uint, uint pin, uint count, bool output) {
    auto& r = picottl_test::sdk::recording;
    const uint instance = picottl_test::sdk::index(p), base = pio_get_gpio_base(p);
    if (pin < base || pin + count > base + 32) { ++r.invalidPinCalls; return; }
    const auto mask = ((1u << count) - 1u) << (pin - base);
    if (output) r.directionMask[instance] |= mask;
    else r.directionMask[instance] &= ~mask;
    ++r.calls; ++r.pinDirections;
}
inline void pio_gpio_init(PIO p, uint pin) {
    auto& r = picottl_test::sdk::recording;
    assert(pin < r.gpioFunction.size());
    r.gpioFunction[pin] = static_cast<int>(picottl_test::sdk::index(p)) + 1;
    ++r.calls; ++r.gpioConfigurations;
}
inline void gpio_set_drive_strength(uint, uint) { ++picottl_test::sdk::recording.calls; ++picottl_test::sdk::recording.padConfigurations; }
inline void gpio_set_slew_rate(uint, uint) { ++picottl_test::sdk::recording.calls; ++picottl_test::sdk::recording.padConfigurations; }
inline uint pio_encode_jmp(uint pc) { if (pc >= 32) ++picottl_test::sdk::recording.invalidPc; return 0; }
inline uint pio_encode_pull(bool, bool) { return 0; }
inline uint pio_encode_mov(uint, uint) { return 0; }
inline uint pio_encode_out(uint, uint) { return 0; }
inline void sm_config_set_out_pins(pio_sm_config* c, uint pin, uint count) { c->outPin = static_cast<int>(pin); c->outCount = count; }
inline void sm_config_set_in_pins(pio_sm_config* c, uint pin) { c->inPin = static_cast<int>(pin); }
inline void sm_config_set_out_shift(pio_sm_config*, bool, bool, uint) {}
inline void sm_config_set_fifo_join(pio_sm_config*, uint) {}
// Mirrors SDK 2.3.0 pio_calculate_clkdiv8_from_float, including optional
// nearest rounding. Record an unsafe upper-endpoint conversion instead of
// executing it; production must use explicit integer/fraction encoding there.
inline void pio_calculate_clkdiv8_from_float(float divider, std::uint32_t* integer,
                                            std::uint8_t* fraction) {
    auto& r = picottl_test::sdk::recording;
    if (!(divider >= 1 && divider <= 65536)) ++r.invalidDividerCalls;
    if (r.roundDividerNearest) divider += 0.5f / 256.0f;
    if (!(divider >= 1 && divider < 65536)) {
        ++r.floatEndpointCalls;
        *integer = 0; *fraction = 0;
        return;
    }
    *integer = static_cast<std::uint16_t>(divider);
    *fraction = static_cast<std::uint8_t>((divider - static_cast<float>(*integer)) * 256.0f);
}
inline void sm_config_set_clkdiv(pio_sm_config* c, float divider) {
    auto& r = picottl_test::sdk::recording;
    ++r.dividerCalls;
    std::uint32_t integer;
    std::uint8_t fraction;
    pio_calculate_clkdiv8_from_float(divider, &integer, &fraction);
    c->clkdiv = (integer << 16) | (static_cast<std::uint32_t>(fraction) << 8);
    c->divider = divider; // requested value, alongside the actual encoded register
    c->fullPeriodEncoded = false;
}
inline void sm_config_set_clkdiv_int_frac8(pio_sm_config* c, std::uint32_t integer,
                                          std::uint8_t fraction) {
    auto& r = picottl_test::sdk::recording;
    ++r.dividerCalls; ++r.integerDividerCalls;
    if (integer > 65535 || (integer == 0 && fraction != 0)) ++r.invalidDividerCalls;
    c->clkdiv = (integer << 16) | (static_cast<std::uint32_t>(fraction) << 8);
    c->fullPeriodEncoded = integer == 0 && fraction == 0;
    c->divider = c->fullPeriodEncoded ? 65536.0f
        : static_cast<float>(integer) + static_cast<float>(fraction) / 256.0f;
}

inline bool dma_channel_is_claimed(uint ch) { assert(ch < NUM_DMA_CHANNELS); return picottl_test::sdk::recording.dmaClaimed[ch]; }
inline void dma_channel_claim(uint ch) { assert(!dma_channel_is_claimed(ch)); picottl_test::sdk::recording.dmaClaimed[ch] = true; ++picottl_test::sdk::recording.calls; }
inline void dma_channel_unclaim(uint ch) { assert(dma_channel_is_claimed(ch)); picottl_test::sdk::recording.dmaClaimed[ch] = false; ++picottl_test::sdk::recording.calls; ++picottl_test::sdk::recording.releases; }
inline bool dma_channel_is_busy(uint ch) { return (dma_hw->ch[ch].ctrl_trig & DMA_CH0_CTRL_TRIG_BUSY_BITS) != 0; }
inline dma_channel_config dma_channel_get_default_config(uint ch) { return {1u | (ch << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB)}; }
inline void channel_config_set_chain_to(dma_channel_config* c, uint ch) { c->ctrl = (c->ctrl & ~DMA_CH0_CTRL_TRIG_CHAIN_TO_BITS) | (ch << DMA_CH0_CTRL_TRIG_CHAIN_TO_LSB); }
inline void channel_config_set_transfer_data_size(dma_channel_config*, uint) {}
inline void channel_config_set_read_increment(dma_channel_config*, bool) {}
inline void channel_config_set_write_increment(dma_channel_config*, bool) {}
inline void channel_config_set_dreq(dma_channel_config*, uint) {}
inline void channel_config_set_bswap(dma_channel_config*, bool) {}
inline void dma_channel_configure(uint ch, const dma_channel_config* c, volatile void* write, const volatile void* read, std::uint32_t count, bool trigger) {
    assert(!trigger); // The engine must configure the whole group before triggering.
    auto& r = picottl_test::sdk::recording;
    auto& call = r.dma[ch];
    call.read = read; call.write = write; call.count = count; ++call.configurations;
    dma_hw->ch[ch].ctrl_trig = c->ctrl; ++r.calls;
}
inline void dma_channel_set_read_addr(uint ch, const volatile void* read, bool trigger) { assert(!trigger); picottl_test::sdk::recording.dma[ch].read = read; ++picottl_test::sdk::recording.calls; }
inline void dma_channel_set_trans_count(uint ch, std::uint32_t count, bool trigger) {
    auto& r = picottl_test::sdk::recording;
    r.dma[ch].count = count; ++r.calls;
    if (trigger) { ++r.triggers; assert((dma_hw->ch[ch].ctrl_trig & 1u) != 0); dma_hw->ch[ch].ctrl_trig |= DMA_CH0_CTRL_TRIG_BUSY_BITS; }
}
inline void hw_write_masked(std::uint32_t* ptr, std::uint32_t value, std::uint32_t mask) { *ptr = (*ptr & ~mask) | (value & mask); ++picottl_test::sdk::recording.calls; }
inline void tight_loop_contents() { assert(false && "Recording SDK never leaves an abort pending"); }

inline constexpr pio_program_t ttl_timing_program{0}, ttl_pixels_program{1};
inline constexpr pio_program_t ttl_pixels2_program{2}, ttl_pixels4_program{3}, ttl_pixels8_program{4};
inline pio_sm_config ttl_timing_program_get_default_config(uint) { return {}; }
inline pio_sm_config ttl_pixels_program_get_default_config(uint) { return {}; }
inline pio_sm_config ttl_pixels2_program_get_default_config(uint) { return {}; }
inline pio_sm_config ttl_pixels4_program_get_default_config(uint) { return {}; }
inline pio_sm_config ttl_pixels8_program_get_default_config(uint) { return {}; }
