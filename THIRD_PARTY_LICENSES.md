# Third-party data in PicoTTL

PicoTTL's code is original work under the MIT license (see LICENSE).
PicoTTL also includes bitmap character data derived from historical
IBM PC character generator ROMs and DOS font resources, included for
compatibility with original IBM hardware. The purpose of this document
is to record the exact provenance of every asset that was **not**
created for PicoTTL - not to argue legal conclusions, which are for
rights holders, lawyers and courts. Nothing here is silently assumed
to be free.

## Font bitmap data

The MIT license text does not apply to the glyph bitmaps described
below. The project's understanding - shared broadly across the
retrocomputing ecosystem (DOS emulators, font packs, BIOS
re-creations) - is that raw bitmap typeface data of this era is
generally not treated as protectable subject matter in the United
States (see 37 C.F.R. § 202.1(e) and U.S. Copyright Office practice
regarding typeface designs), and that the European design-right terms
that once covered typefaces appear to have expired for these 1980s
designs. This is the project's good-faith understanding, documented
transparently so downstream users can make their own assessment; it is
not a legal determination.

**Good-faith clause.** To the best of the project's knowledge, the
redistribution of the generated bitmap character data is lawful. If
any rights holder believes that material included in this project
infringes their rights, please open an issue or contact the maintainer
so the matter can be reviewed - and, where appropriate, the material
removed or replaced - promptly.

### CP437 glyph data (IBM EGA character ROM)

| | |
|---|---|
| Files | `src/fonts/IbmMda9x14.cpp`, `src/fonts/IbmCga8x8.cpp`, `src/fonts/IbmEga8x14.cpp` (generated; see each header) |
| Generated from | `ibm-ega__8x14.bin`, `ibm-ega__8x8.bin` (raw ROM dumps, not committed to this repository) |
| Immediate source | [spacerace/romfont](https://github.com/spacerace/romfont) (`font-bin/` directory) |
| Original source | IBM EGA adapter character ROM (1984). The 8x14 set is byte-identical to IBM's VGA 8x14 ROM font (verified, 0/256 glyphs differ). |
| License of the immediate source | **None declared.** The repository is an extraction/curation effort; its author claims no authorship over the extracted bitmaps. PicoTTL uses two individual dumps, not the collection's selection or arrangement. |
| Rights in the underlying data | The project is not aware of any license ever granted by IBM for these bitmaps; they are included on the good-faith understanding described above. |
| Attribution required | No attribution requirement is known to the project; attribution is provided anyway (here, in the generated files, and in README) as good practice. |

### CP850 glyph data (IBM PC-DOS 2000 codepage file)

| | |
|---|---|
| Files | The generated `src/fonts/IbmMda9x14Cp850.cpp`, `src/fonts/IbmCga8x8Cp850.cpp`, `src/fonts/IbmEga8x14Cp850.cpp`. The raw inputs (`CP850.F14`, `CP850.F08`) are **not committed** - they are regeneration inputs, not build inputs; the generator documentation records their download URLs. |
| Immediate source | [viler-int10h/vga-text-mode-fonts](https://github.com/viler-int10h/vga-text-mode-fonts) (`FONTS/SYSTEM/PCDOS2K/`) |
| Original source | CP850 fonts from IBM PC-DOS 2000's codepage file (`EGA.CPI` lineage; glyph designs date to DOS 3.3, 1987). |
| License of the immediate source | **None declared** (same situation and same reasoning as above). |
| Rights in the underlying data | IBM; same good-faith understanding as for the CP437 data above. |
| Attribution required | No attribution requirement is known to the project; attribution is provided anyway as good practice. |

## Build system

| | |
|---|---|
| File | `pico_sdk_import.cmake` |
| Source | Verbatim copy of `external/pico_sdk_import.cmake` from the [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk), as its own header instructs ("This can be dropped into an external project"). |
| License | **BSD-3-Clause**, Copyright 2020 Raspberry Pi (Trading) Ltd. The full license text is embedded in the file itself, so the required notice travels with the code. |
| Compatible with MIT distribution | Yes (BSD-3-Clause and MIT coexist; each file keeps its own notice). |

## Design inspiration

Example 45's visual concept was inspired by
[ybuzoku/MatrixScreensaver](https://github.com/ybuzoku/MatrixScreensaver),
an IBM PC/XT MDA text-mode screensaver written by Yll Buzoku and
distributed under the BSD-3-Clause license. PicoTTL does not include or
derive from its assembly source or binaries: the example uses an original
algorithm written against PicoTTL's framebuffer and text APIs. The source
is credited because it identified the IBM 5151/P39 persistence effect as
the intended hardware experience.

### What is NOT third-party

- `tools/generate_mda_font.py`, `generate_cga_font.py`,
  `generate_ega_font.py`, `verify_cp850.py` - original PicoTTL code
  (MIT).
- The 9th-column expansion, underline metadata and file layout of the
  generated sources - original PicoTTL work applied to the data above.
- Bayer dithering matrices, CGA/EGA palette RGB values, IBM 6845
  timing numbers, the CP437/CP850 difference tables - mathematical
  constants and historical compatibility data, documented at their
  points of use.

## Images and video

None yet. All screenshots, photographs and videos added to this
repository will be original work by the project author (MIT, like the
rest of the repository) unless explicitly annotated otherwise here.
