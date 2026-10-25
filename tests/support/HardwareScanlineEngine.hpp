// SPDX-License-Identifier: MIT
#pragma once

// Give the real implementation a distinct test-only class name so it can
// coexist with the device tests' RecordingScanlineEngine in one executable.
#define PioScanlineEngine HardwareScanlineEngine
#include "picottl/rp2350/PioScanlineEngine.hpp"
#undef PioScanlineEngine
