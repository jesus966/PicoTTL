# Example 43 - Host image display

**The recommended starting point for host-driven graphics on PicoTTL.**
A PC loads any image, converts it, and displays it on a real MDA, CGA
or EGA monitor - without the user writing a single line of firmware.

```
PC image -> conversion (host) -> framebuffer bytes -> USB CDC
         -> PicoTTL framebuffer -> real monitor
```

## Roles

- **Firmware** (`main.cpp`): deliberately tiny. Initializes the
  selected PicoTTL backend, receives complete framebuffers over USB
  CDC, copies bytes into the framebuffer, acknowledges. No decoding,
  no scaling, no color conversion, no compression - the Pico is only a
  display device. One binary serves all four modes (MDA, CGA 640/320,
  EGA) on the **PicoTTL reference connector**: MDA via
  `MdaDisplayDevice`, the CGA and EGA modes via `EgaDisplayDevice`
  (its CGA-compatible timings drive CGA monitors exactly as IBM's EGA
  card did), each device on its own PIO block.
- **Host** (`host/`, Python): everything intelligent - image loading
  (Pillow), aspect-correct scaling with each mode's **non-square pixel
  aspect ratio**, letterboxing, gamma-naive ordered (Bayer) dithering,
  palette mapping per backend contract (MDA 4 levels, CGA 16 RGBI
  encoded as EGA 200-line pixel values, EGA 64 rgbRGB), MSB-first
  packing matching `picottl::PackedFramebuffer`.

## Usage

1. Flash `picottl_example_43_host_image.uf2` and wire the PicoTTL
   reference connector (see the header of `main.cpp`; one wiring
   serves all monitors).
2. Install host dependencies: `pip install pillow numpy pyserial`
3. Run:

```
python host/main.py --port COM5 --mode ega350 photo.jpg
python host/main.py --port COM5 --mode mda diagram.png
```

The image paints top-down as it arrives (~1 MB/s over Full-Speed USB:
roughly 1/16 s for MDA, 1/8 s for CGA 640, ~1/4 s for EGA).

## Protocol (intentionally minimal)

| Command | Meaning | Reply |
|---|---|---|
| `H` | hello / mode table | one info line |
| `M` + index byte | select mode (reclocks, restarts raster) | `OK <fbBytes>` / `ERR …` |
| `F` + u32le length + bytes | full framebuffer | `ACK` / `ERR …` |

No rectangles, no compression, no deltas, no dirty tracking - those
belong to **PicoTTL Stream**, which builds on exactly these concepts
and adds rectangles, compression, delta encoding, adaptive updates and
video (see its protocol specification). This example is pedagogical:
the complete host->PicoTTL pipeline with the smallest possible protocol.
The conversion routines in `host/converter.py` are written to be reused
by PicoTTL Stream nearly unchanged - in particular, ordered dithering
is chosen because it is position-deterministic, the property delta
encoding will later depend on.
