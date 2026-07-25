# Copyright (c) 2026 Jesús Fernández Gamito
# SPDX-License-Identifier: MIT

"""Display any image on a real MDA/CGA/EGA monitor through PicoTTL.

Usage:
  python main.py --port COM5 --mode ega350 photo.jpg
  python main.py --port COM5 --mode mda --dither 32 diagram.png

Requires: pillow, numpy, pyserial. Flash the Example 43 firmware first.
"""

import argparse
import sys
import time

import converter
import image_loader
from transport import PicoLink


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", help="any Pillow-supported image file")
    parser.add_argument("--port", required=True,
                        help="serial port (e.g. COM5, /dev/ttyACM0)")
    parser.add_argument("--mode", default="mda",
                        choices=sorted(converter.MODES))
    parser.add_argument("--dither", type=float, default=48.0,
                        help="ordered-dither strength (0 disables)")
    args = parser.parse_args()

    mode = converter.MODES[args.mode]

    rgb = image_loader.load_rgb(args.image)
    fitted = image_loader.fit_to_mode(rgb, mode.width, mode.height,
                                      mode.pixel_aspect)
    framebuffer = converter.convert(fitted, mode, args.dither)

    link = PicoLink(args.port)
    try:
        print(link.hello())
        expected = link.set_mode(mode.index)
        if expected != len(framebuffer):
            print(f"error: firmware expects {expected} bytes, "
                  f"converter produced {len(framebuffer)}", file=sys.stderr)
            return 1
        start = time.monotonic()
        link.send_frame(framebuffer)
        elapsed = time.monotonic() - start
        rate = len(framebuffer) / elapsed / 1024
        print(f"sent {len(framebuffer)} bytes in {elapsed:.2f} s "
              f"({rate:.0f} KiB/s) - image on screen")
    finally:
        link.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
