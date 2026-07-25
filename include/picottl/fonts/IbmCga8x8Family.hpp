// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// fonts/IbmCga8x8Family.hpp
// The IBM CGA 8x8 font family: one dataset per supported code page.
//
// PUBLIC API (opt-in resource) - include explicitly. Including this
// header knowingly links EVERY member dataset into flash (currently
// CP437 + CP850 = 4096 bytes); applications using a single code page
// should include that dataset's own header instead.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include "picottl/Font.hpp"
#include "picottl/fonts/IbmCga8x8.hpp"
#include "picottl/fonts/IbmCga8x8Cp850.hpp"

namespace picottl::fonts {

namespace detail {
inline constexpr const Font* kIbmCga8x8FamilyMembers[] = {
    &kIbmCga8x8,      // CodePage::Cp437 (the default)
    &kIbmCga8x8Cp850, // CodePage::Cp850
};
} // namespace detail

/// The IBM CGA 8x8 family: identical-metrics datasets differing only
/// in code page, enabling runtime switching via Text/Terminal
/// setCodePage() without recreating either (see FontFamily).
inline constexpr FontFamily kIbmCga8x8Family{
    detail::kIbmCga8x8FamilyMembers,
    sizeof detail::kIbmCga8x8FamilyMembers /
        sizeof detail::kIbmCga8x8FamilyMembers[0]};

} // namespace picottl::fonts
