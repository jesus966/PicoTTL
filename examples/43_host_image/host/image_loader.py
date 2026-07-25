# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

"""Image loading, aspect-correct scaling and letterboxing for PicoTTL.

The classic TTL monitors do NOT have square pixels, so fitting an image
"preserving aspect ratio" must account for each mode's pixel aspect
ratio (PAR): the image is fitted in *displayed* space (a 4:3 tube) and
then resampled onto the mode's non-square pixel grid.
"""

from PIL import Image
import numpy as np


def load_rgb(path: str) -> np.ndarray:
    """Loads any Pillow-supported image as an RGB numpy array (h, w, 3)."""
    with Image.open(path) as img:
        return np.asarray(img.convert("RGB"))


def fit_to_mode(rgb: np.ndarray, width: int, height: int,
                pixel_aspect: float) -> np.ndarray:
    """Fits an image into a (width x height) framebuffer grid whose
    pixels have the given aspect ratio (pixel width / pixel height as
    displayed), preserving the image's displayed aspect ratio and
    letterboxing with black. Returns an (height, width, 3) array."""
    src_h, src_w = rgb.shape[:2]
    # Displayed size of the full target grid, in square-pixel units.
    disp_w = width * pixel_aspect
    disp_h = float(height)
    scale = min(disp_w / src_w, disp_h / src_h)
    # Content size, converted back into framebuffer pixels.
    content_w = max(1, round(src_w * scale / pixel_aspect))
    content_h = max(1, round(src_h * scale))
    content = Image.fromarray(rgb).resize((content_w, content_h),
                                          Image.LANCZOS)
    canvas = Image.new("RGB", (width, height), (0, 0, 0))
    canvas.paste(content, ((width - content_w) // 2,
                           (height - content_h) // 2))
    return np.asarray(canvas)
