// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// fonts/IbmMda9x14Family.hpp
// The IBM MDA 9x14 font family: one dataset per supported code page.
//
// PUBLIC API (opt-in resource) - include explicitly. Including this
// header knowingly links EVERY member dataset into flash (currently
// CP437 + CP850 = 14336 bytes); applications using a single code page
// should include that dataset's own header instead.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "picottl/Font.hpp"
#include "picottl/fonts/IbmMda9x14.hpp"
#include "picottl/fonts/IbmMda9x14Cp850.hpp"

namespace picottl::fonts {

namespace detail {
inline constexpr const Font* kIbmMda9x14FamilyMembers[] = {
    &kIbmMda9x14,      // CodePage::Cp437 (the default)
    &kIbmMda9x14Cp850, // CodePage::Cp850
};
} // namespace detail

/// The IBM MDA 9x14 family: identical-metrics datasets differing only
/// in code page, enabling runtime switching via Text/Terminal
/// setCodePage() without recreating either (see FontFamily).
///
/// Usage:
///   picottl::Text text(display.graphics(),
///                      picottl::fonts::kIbmMda9x14Family);
///   terminal.setCodePage(picottl::CodePage::Cp850);
inline constexpr FontFamily kIbmMda9x14Family{
    detail::kIbmMda9x14FamilyMembers,
    sizeof detail::kIbmMda9x14FamilyMembers /
        sizeof detail::kIbmMda9x14FamilyMembers[0]};

} // namespace picottl::fonts
