// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// Font.hpp
// Immutable monospace bitmap font resource.
//
// PUBLIC API - FROZEN. Platform-independent.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <cstdint>

#include "picottl/CodePage.hpp"
#include "picottl/MonochromeBitmap.hpp"

namespace picottl {

/// Immutable monospace bitmap font resource.
///
/// A plain descriptor over caller-owned, typically flash-resident glyph
/// data: `glyphCount` glyphs starting at glyph index 0, each stored
/// as `glyphHeight` rows of `glyphStrideBytes` (MSB = leftmost pixel,
/// same convention as MonochromeBitmap).
///
/// A Font is an ordered collection of glyphs plus the immutable
/// metadata that makes it self-describing: cell dimensions, glyph
/// count, underline row and the code page its glyph order encodes
/// (see CodePage). Following the original IBM model, each code page is
/// a complete independent dataset in native byte order - byte value =
/// glyph index - rather than a base font with supplements.
///
/// Fonts are reusable resources owned by nobody: Text (and any future
/// consumer) merely references them. Future fonts (other code pages,
/// user-defined sets) are simply new Font instances. Proportional
/// fonts, if ever desired, will arrive as an additive abstraction -
/// this struct intentionally models fixed-cell fonts only.
///
/// Metrics policy: Font describes glyph data, never rendering policy.
/// Line spacing beyond the cell is layout policy and belongs to the
/// renderer (e.g. a future Text::setLineSpacing()). Per-font metrics
/// with real consumers (e.g. an underline row when MDA attributes
/// arrive) are added as new fields with default initializers, which
/// keeps every existing aggregate initialization valid - additive
/// evolution is structurally guaranteed.
struct Font {
    /// Sentinel for underlineRow: the font defines no underline position.
    static constexpr std::uint8_t kNoUnderlineRow = 0xFF;

    const std::uint8_t* glyphData = nullptr; ///< All glyphs, contiguous.
    std::uint16_t glyphWidth = 0;       ///< Cell width in pixels.
    std::uint16_t glyphHeight = 0;      ///< Cell height in pixels.
    std::uint16_t glyphStrideBytes = 0; ///< Bytes per glyph row.
    std::uint16_t glyphCount = 0;       ///< Glyphs available, from code 0.
    /// Glyph row on which text underline is drawn (kNoUnderlineRow when
    /// the font defines none). MDA fonts use row 12, matching the
    /// original hardware attribute. Added as a defaulted field: every
    /// pre-existing aggregate initialization remains valid.
    std::uint8_t underlineRow = kNoUnderlineRow;
    /// The code page this dataset's glyph order encodes (metadata; see
    /// CodePage). Defaulted field - additive evolution as usual.
    CodePage codePage = CodePage::Cp437;

    /// Storage size of one glyph in bytes.
    constexpr std::size_t bytesPerGlyph() const {
        return static_cast<std::size_t>(glyphStrideBytes) * glyphHeight;
    }

    /// Bitmap view of one glyph. Out-of-range indices yield an empty
    /// bitmap; renderers substitute their fallback (Text paints a blank
    /// cell). The parameter was widened from `unsigned char` to
    /// `std::uint16_t` (source-compatible; values 0..255 unchanged) so
    /// fonts larger than 256 glyphs - future Unicode subsets - index
    /// naturally.
    constexpr MonochromeBitmap glyph(std::uint16_t index) const {
        if (glyphData == nullptr || index >= glyphCount) {
            return MonochromeBitmap{};
        }
        return MonochromeBitmap{glyphData + index * bytesPerGlyph(),
                                glyphWidth, glyphHeight, glyphStrideBytes};
    }
};

/// A family of related font datasets sharing identical metrics
/// (glyphWidth, glyphHeight, glyphStrideBytes) and differing only in
/// the character set they encode - today the code page, tomorrow
/// possibly localized variants or Unicode subsets.
///
/// The identical-metrics invariant is what allows switching members at
/// runtime without recreating Text or Terminal: the cell grid, and
/// therefore every geometry snapshot, is unchanged.
///
/// A FontFamily is a plain collection; lookup keys off each member's
/// own metadata (Font::codePage), so adding a new code page to a
/// family never changes this struct. Families are opt-in resources
/// like fonts: including a family header knowingly links ALL member
/// datasets into flash (see picottl/fonts/*Family.hpp).
struct FontFamily {
    const Font* const* members = nullptr; ///< Member datasets.
    std::size_t memberCount = 0;          ///< Entries in `members`.

    /// The member encoding `page`, or nullptr if the family has none.
    constexpr const Font* font(CodePage page) const {
        for (std::size_t i = 0; i < memberCount; ++i) {
            if (members[i] != nullptr && members[i]->codePage == page) {
                return members[i];
            }
        }
        return nullptr;
    }
};

} // namespace picottl
