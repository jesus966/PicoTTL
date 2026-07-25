// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// CodePage.hpp
// IBM/DOS code page identifiers - immutable font dataset metadata.
//
// PUBLIC API - FROZEN (new code pages are added as additive
// enumerators). Platform-independent.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace picottl {

/// Identifies the character encoding a font dataset renders: which
/// visible character each byte value 0..255 produces.
///
/// CodePage is METADATA, not a mapping object. PicoTTL follows the
/// original IBM model: every font dataset is a complete, independent
/// 256-glyph set stored in its code page's native byte order, so the
/// byte value IS the glyph index (identity mapping). Selecting a code
/// page therefore means selecting a font dataset - exactly what DOS's
/// MODE CON CP SELECT did when it loaded a font from EGA.CPI into the
/// character generator. See Font::codePage, FontFamily and
/// Text::setCodePage().
///
/// A future multi-byte pipeline (e.g. UTF-8 -> Unicode codepoint ->
/// per-font index mapping -> glyph) arrives as additive layers; this
/// enum then identifies the single-byte legacy encodings within it.
///
/// HISTORICAL NOTE. Code page 437 is the character set of the original
/// IBM PC's ROM (1981): US ASCII plus box-drawing characters, some
/// Western-European lowercase accents, Greek letters and mathematical
/// symbols. It could not spell many Western-European words correctly -
/// it lacks most uppercase accented letters (no A-acute, no
/// O-circumflex...). With DOS 3.3 (1987) IBM introduced switchable code
/// pages, and CP850 "Multilingual (Latin-1)" became the de facto
/// standard for DOS in Western Europe: it sacrifices most of CP437's
/// double/single mixed box-drawing characters and Greek letters to
/// cover the full Latin-1 repertoire (all accented vowels, eth, thorn,
/// multiplication sign, copyright...). Almost all characters shared by
/// both pages sit at the SAME byte positions, so plain text and framed
/// TUIs largely survive a page switch intact.
///
/// PicoTTL's default is CP437 everywhere - the historically correct
/// choice for IBM MDA/CGA/EGA hardware. CP850 is an opt-in capability
/// for later DOS-era and international applications.
enum class CodePage : std::uint8_t {
    Cp437, ///< IBM PC original character set (1981). The default.
    Cp850, ///< DOS "Multilingual (Latin-1)", Western Europe (DOS 3.3+).
};

} // namespace picottl
