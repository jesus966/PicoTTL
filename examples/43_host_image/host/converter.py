# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

"""RGB -> PicoTTL framebuffer conversion for MDA, CGA and EGA.

Implements each backend's DOCUMENTED public color contract:
  MDA    Packed2, 4 brightness levels (verbatim indices; perceived
         order on a 5151: 0 black < 2 intensity-only < 1 normal
         < 3 bright)
  CGA    Packed8 via the EGA backend's CGA-compatible timings (the
         PicoTTL reference connector, exactly how IBM's EGA card drove
         CGA monitors): the framebuffer value is the EGA 200-line
         pixel value (i & 7) | ((i & 8) << 1) of the IBM RGBI color
         number i, placing intensity on Secondary Green = DE-9 pin 6
  EGA    Packed8, pixel value = IBM EGA 6-bit color value in the low
         six bits (rgbRGB: primaries 2/3 amplitude, secondaries 1/3)

Packing is MSB-first (leftmost pixel in the most significant bits),
matching picottl::PackedFramebuffer exactly.

Ordered (Bayer) dithering is used deliberately: it is
position-deterministic, so static image regions always produce
byte-identical framebuffers - the property PicoTTL Stream's delta
encoding will rely on. These routines are designed for reuse there.
"""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class Mode:
    index: int          # firmware mode index ('M' command payload)
    name: str
    width: int
    height: int
    bpp: int
    pixel_aspect: float  # displayed pixel width / height (4:3 tube)


MODES = {
    "mda":    Mode(0, "mda",    720, 350, 2, (4 / 3) * 350 / 720),
    "cga640": Mode(1, "cga640", 640, 200, 8, (4 / 3) * 200 / 640),
    "cga320": Mode(2, "cga320", 320, 200, 8, (4 / 3) * 200 / 320),
    "ega350": Mode(3, "ega350", 640, 350, 8, (4 / 3) * 350 / 640),
}

# 8x8 Bayer matrix, normalized to -0.5 .. +0.5.
_BAYER8 = (1 / 64) * np.array(
    [[0, 32, 8, 40, 2, 34, 10, 42],
     [48, 16, 56, 24, 50, 18, 58, 26],
     [12, 44, 4, 36, 14, 46, 6, 38],
     [60, 28, 52, 20, 62, 30, 54, 22],
     [3, 35, 11, 43, 1, 33, 9, 41],
     [51, 19, 59, 27, 49, 17, 57, 25],
     [15, 47, 7, 39, 13, 45, 5, 37],
     [63, 31, 55, 23, 61, 29, 53, 21]], dtype=np.float32) - 0.5


def _bayer_tile(h: int, w: int) -> np.ndarray:
    reps = (-(-h // 8), -(-w // 8))
    return np.tile(_BAYER8, reps)[:h, :w]


# CGA: IBM RGBI color numbers 0..15 (canonical sRGB approximations,
# including the 5153's brown for color 6).
CGA_PALETTE = np.array(
    [(0, 0, 0), (0, 0, 170), (0, 170, 0), (0, 170, 170),
     (170, 0, 0), (170, 0, 170), (170, 85, 0), (170, 170, 170),
     (85, 85, 85), (85, 85, 255), (85, 255, 85), (85, 255, 255),
     (255, 85, 85), (255, 85, 255), (255, 255, 85), (255, 255, 255)],
    dtype=np.float32)

# EGA: value v in 0..63, gun level = 2*primary + secondary in 0..3,
# displayed intensity = level * 85.
def _ega_palette() -> np.ndarray:
    values = np.arange(64)
    r = 2 * ((values >> 2) & 1) + ((values >> 5) & 1)
    g = 2 * ((values >> 1) & 1) + ((values >> 4) & 1)
    b = 2 * (values & 1) + ((values >> 3) & 1)
    return (np.stack([r, g, b], axis=1) * 85.0).astype(np.float32)


EGA_PALETTE = _ega_palette()

# CGA modes run on the EGA backend's CGA-compatible timings: RGBI
# color number i travels as the EGA 200-line pixel value, with
# intensity on Secondary Green (bit 4).
CGA_TO_EGA200 = np.array([(i & 7) | ((i & 8) << 1) for i in range(16)],
                         dtype=np.uint8)

# MDA: 4 perceived brightness levels; framebuffer indices in ascending
# perceived brightness order (validated on a real 5151).
MDA_LEVEL_INDICES = np.array([0, 2, 1, 3], dtype=np.uint8)


def _nearest_palette(rgb: np.ndarray, palette: np.ndarray,
                     dither: float) -> np.ndarray:
    """Per-pixel nearest palette index with ordered dithering."""
    h, w = rgb.shape[:2]
    noise = _bayer_tile(h, w)[..., None] * dither
    candidate = np.clip(rgb.astype(np.float32) + noise, 0, 255)
    # Distances to every palette entry: (h, w, n).
    diff = candidate[:, :, None, :] - palette[None, None, :, :]
    return np.argmin((diff * diff).sum(axis=3), axis=2).astype(np.uint8)


def _pack_bits(indices: np.ndarray, bpp: int) -> bytes:
    """Packs per-pixel indices MSB-first into framebuffer bytes."""
    if bpp == 8:
        return indices.astype(np.uint8).tobytes()
    per_byte = 8 // bpp
    h, w = indices.shape
    assert w % per_byte == 0
    grouped = indices.reshape(h, w // per_byte, per_byte).astype(np.uint16)
    out = np.zeros((h, w // per_byte), dtype=np.uint16)
    for i in range(per_byte):
        out |= grouped[:, :, i] << (8 - bpp * (i + 1))
    return out.astype(np.uint8).tobytes()


def convert(rgb: np.ndarray, mode: Mode, dither: float = 48.0) -> bytes:
    """Converts an (h, w, 3) RGB array at the mode's exact resolution
    into PicoTTL framebuffer bytes."""
    assert rgb.shape[:2] == (mode.height, mode.width), "resize first"
    if mode.name == "mda":
        # Rec.601 luma, quantized to 4 levels with ordered dithering.
        luma = rgb.astype(np.float32) @ np.array([0.299, 0.587, 0.114],
                                                 dtype=np.float32)
        noise = _bayer_tile(mode.height, mode.width) * dither
        level = np.clip((luma + noise) / 255.0 * 3.0 + 0.5, 0,
                        3).astype(np.uint8)
        indices = MDA_LEVEL_INDICES[level]
    elif mode.name in ("cga640", "cga320"):
        rgbi = _nearest_palette(rgb, CGA_PALETTE, dither)
        indices = CGA_TO_EGA200[rgbi]
    else:
        indices = _nearest_palette(rgb, EGA_PALETTE, dither)
    return _pack_bits(indices, mode.bpp)
