// PicoTTL CRT Diagnostics - BOOTSEL button input
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include "BootselButton.hpp"

#include <cstdint>

#include "hardware/structs/ioqspi.h"
#include "hardware/structs/sio.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

bool __no_inline_not_in_flash_func(bootselButtonPressed)() {
    const uint kCsPinIndex = 1;

    // Interrupt handlers may live in flash, which is about to become
    // unreachable for a few microseconds.
    const std::uint32_t flags = save_and_disable_interrupts();

    // Float the flash chip-select pad so the button can pull it low.
    hw_write_masked(&io_qspi_hw->io[kCsPinIndex].ctrl,
                    GPIO_OVERRIDE_LOW << IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_BITS);

    // Let the pad settle (~1-2 us; loop kept in RAM).
    for (volatile int i = 0; i < 1000; ++i) {
    }

#if PICO_RP2040
    const std::uint32_t csBit = 1u << 1;
#else
    const std::uint32_t csBit = SIO_GPIO_HI_IN_QSPI_CSN_BITS;
#endif
    const bool pressed = (sio_hw->gpio_hi_in & csBit) == 0;

    // Restore the chip select and flash access.
    hw_write_masked(&io_qspi_hw->io[kCsPinIndex].ctrl,
                    GPIO_OVERRIDE_NORMAL << IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_LSB,
                    IO_QSPI_GPIO_QSPI_SD1_CTRL_OEOVER_BITS);

    restore_interrupts(flags);
    return pressed;
}
