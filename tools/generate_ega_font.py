#!/usr/bin/env python3
# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

"""Generates the PicoTTL EGA 8x14 font source from an 8x14 ROM font dump.

PROVENANCE NOTE: the input .bin files are historical font data
downloaded separately - they are REGENERATION INPUTS ONLY. They are
not part of the normal build (the generated .cpp files are committed
and compiled directly), and they are NOT redistributed inside the
PicoTTL repository. Their origin and the basis on which the generated
data is included are documented in THIRD_PARTY_LICENSES.md.

Input: a raw binary of 256 glyphs x 14 rows x 1 byte (3584 bytes), as
extracted from an IBM EGA character ROM (see e.g. the font-bin/
directory of https://github.com/spacerace/romfont). The ibm-ega__8x14
dump has been verified byte-identical to IBM's VGA 8x14 ROM font
(0/256 glyphs differ), so this source is authentic for both EGA and
VGA text modes.

Unlike the MDA generator there is no 9th-column expansion: EGA cells
are exactly 8 pixels wide, one byte per row, MSB-first (bit 7 =
leftmost pixel). The output font sets underlineRow = 12, matching the
MDA convention for the same 14-line glyph set (real EGA color text had
no underline attribute - PicoTTL keeps the underline attribute and the
underline cursor functional across backends, a deliberate
standards-are-defaults-not-limits decision).

Usage:
    python tools/generate_ega_font.py <rom_8x14.bin> <output.cpp> [cp850]

The optional third argument selects the code page variant: it changes
the generated font name (kIbmEga8x14 -> kIbmEga8x14Cp850) and stamps
the Font's codePage metadata. Each code page is a complete independent
256-glyph dataset in native byte order (the original IBM model - DOS
loaded whole fonts from CPI files).
"""

import sys

GLYPH_COUNT = 256
GLYPH_ROWS = 14

VARIANTS = {
    "cp437": ("", "", "code page 437 (IBM PC original)"),
    "cp850": ("Cp850", ", CodePage::Cp850",
              "code page 850 (DOS Multilingual/Latin-1)"),
}

HEADER = """\
// PicoTTL - TTL video framework for the Raspberry Pi Pico 2 (RP2350)
//
// IbmEga8x14{suffix}.cpp
// GENERATED FILE - DO NOT EDIT. See tools/generate_ega_font.py.
//
// Source: 8x14 IBM character font dump ({source}), {page}. One byte
// per glyph row, MSB-first (bit 7 = leftmost pixel).
//
// SPDX-License-Identifier: MIT

#include "picottl/fonts/IbmEga8x14{suffix}.hpp"

namespace picottl::fonts {{

namespace {{

const std::uint8_t kGlyphData[{size}] = {{
"""

FOOTER = """\
}};

}} // namespace

// underlineRow = 12: real EGA color text had no underline attribute;
// PicoTTL keeps underline (and the underline cursor) functional on the
// row the 14-line glyph set uses on MDA.
const Font kIbmEga8x14{suffix}{{kGlyphData, 8, 14, 1, {count}, 12{page_init}}};

}} // namespace picottl::fonts
"""


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    rom_path, out_path = sys.argv[1], sys.argv[2]
    variant = sys.argv[3].lower() if len(sys.argv) == 4 else "cp437"
    if variant not in VARIANTS:
        sys.exit(f"error: unknown code page variant '{variant}'")
    suffix, page_init, page = VARIANTS[variant]

    with open(rom_path, "rb") as f:
        rom = f.read()
    if len(rom) < GLYPH_COUNT * GLYPH_ROWS:
        sys.exit(f"error: {rom_path} has {len(rom)} bytes, "
                 f"expected at least {GLYPH_COUNT * GLYPH_ROWS}")

    lines = [HEADER.format(source=rom_path.replace("\\", "/").split("/")[-1],
                           size=GLYPH_COUNT * GLYPH_ROWS,
                           suffix=suffix, page=page)]
    for code in range(GLYPH_COUNT):
        rows = rom[code * GLYPH_ROWS:(code + 1) * GLYPH_ROWS]
        printable = chr(code) if 32 <= code < 127 and code != 92 else ""
        label = f"0x{code:02X}" + (f" '{printable}'" if printable else "")
        half = len(rows) // 2
        first = ", ".join(f"0x{v:02X}" for v in rows[:half])
        second = ", ".join(f"0x{v:02X}" for v in rows[half:])
        lines.append(f"    // {label}\n    {first},\n    {second},\n")
    lines.append(FOOTER.format(count=GLYPH_COUNT, suffix=suffix,
                               page_init=page_init))

    with open(out_path, "w", newline="\n") as f:
        f.write("".join(lines))
    print(f"wrote {out_path} ({GLYPH_COUNT} glyphs, "
          f"{GLYPH_COUNT * GLYPH_ROWS} data bytes)")


if __name__ == "__main__":
    main()
