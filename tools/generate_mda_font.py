#!/usr/bin/env python3
# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

"""Generates the PicoTTL MDA 9x14 font source from an 8x14 ROM font dump.

PROVENANCE NOTE: the input .bin files are historical font data
downloaded separately - they are REGENERATION INPUTS ONLY. They are
not part of the normal build (the generated .cpp files are committed
and compiled directly), and they are NOT redistributed inside the
PicoTTL repository. Their origin and the basis on which the generated
data is included are documented in THIRD_PARTY_LICENSES.md.

Input: a raw binary of 256 glyphs x 14 rows x 1 byte (3584 bytes), as
extracted from IBM MDA/EGA/VGA character ROMs (see e.g. the font-bin/
directory of https://github.com/spacerace/romfont - IBM used the same
14-line glyph set across MDA, EGA and VGA).

The MDA 9th-column hardware rule is baked into the output: character
codes 0xC0..0xDF replicate pixel column 8 into column 9 (horizontal
continuity of box-drawing characters); all other codes get a blank 9th
column. Output rows are 2 bytes wide, MSB-first (bit 7 of byte 0 =
leftmost pixel; bit 7 of byte 1 = 9th pixel).

Usage:
    python tools/generate_mda_font.py <rom_8x14.bin> <output.cpp> [cp850]

The optional third argument selects the code page variant: it changes
the generated font name (kIbmMda9x14 -> kIbmMda9x14Cp850) and stamps
the Font's codePage metadata. Each code page is a complete independent
256-glyph dataset in native byte order (the original IBM model - DOS
loaded whole fonts from CPI files); the 9th-column rule is applied by
code POSITION in every variant, exactly like the hardware circuit.
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
// IbmMda9x14{suffix}.cpp
// GENERATED FILE - DO NOT EDIT. See tools/generate_mda_font.py.
//
// Source: 8x14 IBM character font dump ({source}), {page}.
// Pre-expanded to 9-dot cells with the MDA 9th-column rule (codes
// 0xC0..0xDF replicate column 8; all other codes get a blank 9th
// column). The rule is applied by code POSITION, as in the hardware.
//
// SPDX-License-Identifier: MIT

#include "picottl/fonts/IbmMda9x14{suffix}.hpp"

namespace picottl::fonts {{

namespace {{

const std::uint8_t kGlyphData[{size}] = {{
"""

FOOTER = """\
}};

}} // namespace

// Underline on glyph row 12, matching the original MDA attribute.
const Font kIbmMda9x14{suffix}{{kGlyphData, 9, 14, 2, {count}, 12{page_init}}};

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
                           size=GLYPH_COUNT * GLYPH_ROWS * 2,
                           suffix=suffix, page=page)]
    for code in range(GLYPH_COUNT):
        rows = rom[code * GLYPH_ROWS:(code + 1) * GLYPH_ROWS]
        expanded = []
        for row in rows:
            ninth = 0x80 if (0xC0 <= code <= 0xDF and (row & 0x01)) else 0x00
            expanded += [row, ninth]
        printable = chr(code) if 32 <= code < 127 and code != 92 else ""
        label = f"0x{code:02X}" + (f" '{printable}'" if printable else "")
        half = len(expanded) // 2
        first = ", ".join(f"0x{v:02X}" for v in expanded[:half])
        second = ", ".join(f"0x{v:02X}" for v in expanded[half:])
        lines.append(f"    // {label}\n    {first},\n    {second},\n")
    lines.append(FOOTER.format(count=GLYPH_COUNT, suffix=suffix,
                               page_init=page_init))

    with open(out_path, "w", newline="\n") as f:
        f.write("".join(lines))
    print(f"wrote {out_path} ({GLYPH_COUNT} glyphs, "
          f"{GLYPH_COUNT * GLYPH_ROWS * 2} data bytes)")


if __name__ == "__main__":
    main()
