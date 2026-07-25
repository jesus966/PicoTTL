// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// fonts/IbmEga8x14Family.hpp
// The IBM EGA 8x14 font family: one dataset per supported code page.
//
// PUBLIC API (opt-in resource) - include explicitly. Including this
// header knowingly links EVERY member dataset into flash (currently
// CP437 + CP850 = 7168 bytes); applications using a single code page
// should include that dataset's own header instead.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "picottl/Font.hpp"
#include "picottl/fonts/IbmEga8x14.hpp"
#include "picottl/fonts/IbmEga8x14Cp850.hpp"

namespace picottl::fonts {

namespace detail {
inline constexpr const Font* kIbmEga8x14FamilyMembers[] = {
    &kIbmEga8x14,      // CodePage::Cp437 (the default)
    &kIbmEga8x14Cp850, // CodePage::Cp850
};
} // namespace detail

/// The IBM EGA 8x14 family: identical-metrics datasets differing only
/// in code page, enabling runtime switching via Text/Terminal
/// setCodePage() without recreating either (see FontFamily).
inline constexpr FontFamily kIbmEga8x14Family{
    detail::kIbmEga8x14FamilyMembers,
    sizeof detail::kIbmEga8x14FamilyMembers /
        sizeof detail::kIbmEga8x14FamilyMembers[0]};

} // namespace picottl::fonts
