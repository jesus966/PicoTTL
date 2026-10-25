// SPDX-License-Identifier: MIT
#pragma once

#include <cstdint>

namespace picottl::rp2350::detail {

// RP2350 datasheet 12.6.8.3 / E5: disable EN and CHAIN_TO on the
// entire owned group before issuing one simultaneous abort. Callbacks
// keep the ordering testable without pretending to emulate the DMA bus.
template <typename Disable, typename Abort, typename Pending, typename Wait>
void stopDmaChannels(std::uint32_t mask, Disable disable, Abort abort,
                     Pending pending, Wait wait) {
    if (mask == 0) {
        return;
    }
    for (std::uint8_t channel = 0; channel < 32; ++channel) {
        if ((mask & (std::uint32_t{1} << channel)) != 0) {
            disable(channel);
        }
    }
    abort(mask);
    // As in the SDK's blocking abort, retirement depends on outstanding
    // bus transfers, not PIO DREQ. Never release resources before it finishes.
    while (pending(mask)) {
        wait();
    }
}

} // namespace picottl::rp2350::detail
