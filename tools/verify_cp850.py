#!/usr/bin/env python3
# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

"""Provenance verification: diff the CP850 8x14 font against the
generated CP437 data in src/fonts/IbmEga8x14.cpp. Expect ~220 glyph
differences (the CPI letterforms differ from the EGA ROM even in the
ASCII range - authentic: DOS replaced all 256 glyphs on a code page
switch); the ENCODING-level difference is 47 positions.

CP850.F14 is not committed to the repository (it is a regeneration
input, not a build input, and is not redistributed inside PicoTTL; its
provenance is documented in THIRD_PARTY_LICENSES.md). Download it
first:
  https://raw.githubusercontent.com/viler-int10h/vga-text-mode-fonts/master/FONTS/SYSTEM/PCDOS2K/CP850.F14
"""
import re

with open("src/fonts/IbmEga8x14.cpp") as f:
    text = f.read()
body = text.split("kGlyphData")[1].split("};")[0]
body = "\n".join(l for l in body.splitlines() if not l.strip().startswith("//"))
data = bytes(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", body))
assert len(data) == 3584, len(data)

with open("tools/CP850.F14", "rb") as f:
    cp850 = f.read()
assert len(cp850) == 3584

diff = [c for c in range(256) if data[c*14:(c+1)*14] != cp850[c*14:(c+1)*14]]
low = [c for c in diff if c < 0x80]
print("total differing glyphs:", len(diff))
print("low-half diffs:", [hex(c) for c in low])
print("high-half diffs:", " ".join(f"{c:02X}" for c in diff if c >= 0x80))
