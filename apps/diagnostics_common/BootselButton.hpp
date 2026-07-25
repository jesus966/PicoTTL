// PicoTTL CRT Diagnostics - BOOTSEL button input
//
// Reads the Pico board's BOOTSEL button by briefly floating the flash
// chip-select line (the standard pico-examples technique). Must run
// from RAM with interrupts disabled; the PicoTTL video pipeline is
// unaffected because it runs entirely from PIO + DMA over SRAM.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

/// True while the BOOTSEL button is held down. Costs a few microseconds
/// of flash inaccessibility per call; poll at a modest rate (e.g. every
/// few milliseconds), not in a tight loop.
bool bootselButtonPressed();
