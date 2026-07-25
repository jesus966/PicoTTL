// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// PicoTTL.hpp
// Umbrella header for the platform-independent application API.
//
// Platform backends are intentionally NOT included here; include the one
// your application uses explicitly, e.g.:
//   #include "picottl/rp2350/MdaDisplayDevice.hpp"
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "picottl/AnsiTerminal.hpp"
#include "picottl/CodePage.hpp"
#include "picottl/Color.hpp"
#include "picottl/Display.hpp"
#include "picottl/DisplayDevice.hpp"
#include "picottl/DisplaySurface.hpp"
#include "picottl/Font.hpp"
#include "picottl/Framebuffer.hpp"
#include "picottl/Graphics.hpp"
#include "picottl/MonochromeBitmap.hpp"
#include "picottl/PackedFramebuffer.hpp"
#include "picottl/PixelFormat.hpp"
#include "picottl/Terminal.hpp"
#include "picottl/Text.hpp"
#include "picottl/VideoMode.hpp"

/// Framework version.
#define PICOTTL_VERSION_MAJOR 1
#define PICOTTL_VERSION_MINOR 0
#define PICOTTL_VERSION_PATCH 0
