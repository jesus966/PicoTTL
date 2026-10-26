// PicoTTL CRT Diagnostics - BOOTSEL button input
//
// Reads the Pico board's BOOTSEL button by briefly floating the flash
// chip-select line (the standard pico-examples technique). Must run
// from RAM with interrupts disabled on the calling core. This does not
// park the other core or exclude XIP-reading DMA/other bus masters: the
// application must prevent all other flash access during the call. The
// shipped diagnostics are single-core and scan SRAM buffers, so their PIO/DMA
// pipeline does not need flash during this interval. This reads the button;
// it does not erase/program flash or establish a multicore flash lockout.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

/// True while the BOOTSEL button is held down. Temporarily interrupts flash
/// access; the duration depends on the clock/build, not a fixed deadline.
/// Requires the exclusive-flash-access contract above. Poll at a modest rate
/// (e.g. every few milliseconds), not in a tight loop.
bool bootselButtonPressed();
