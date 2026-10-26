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

### Reception and recovery

The frame length is an unsigned 32-bit little-endian count and must equal
the active mode's size: 63,000, 128,000, 64,000 or 224,000 bytes. Storage
capacity is 224,000 bytes. Command bytes inside a valid payload are pixels,
including `H`, `M` and `F`.

The receiver polls USB without waiting indefinitely for bytes. A pending
mode index, frame header or payload fails after **10 seconds without a byte**.
Each byte renews this inactivity deadline; there is no separate total-transfer
deadline, so slow but progressing transfers remain supported. Timer arithmetic
handles the millisecond counter wrapping. An observed USB CDC disconnection
during reception also fails the transfer. The firmware uses the SDK's polled
connection state (normally DTR); this is not a physical cable-removal detector.

After a timeout, reception disconnect, or **nonzero invalid frame length**,
the receiver reports `ERR` when possible and quarantines all subsequent input
until firmware reset. It does not wait to drain the announced length, execute
command-looking payload bytes, or acknowledge a partial frame. A failed payload
reception clears the active framebuffer to black; rejection before payload
reception leaves the previous image intact. Live scanout can show a partial
image while reception is still in progress, as before.

There is **no in-band resynchronization marker** in this protocol. A timeout,
`H`, a quiet interval, or toggling DTR/reopening the serial port cannot establish
that delayed bytes belong to a new command. Recovery requires stopping the old
sender, closing its port, resetting/power-cycling the Pico, and opening a fresh
connection with no old queued writes. Resetting while an old sender continues
streaming does not establish a clean session.

Two rejections have a known boundary: a zero-length `F` has no declared body,
and an invalid `M` index occupies exactly one byte. Both return `ERR` and allow
the next command. Idle connections have no receive timeout; a clean close after
a complete command/acknowledgement can be reopened normally.

The Python sender's valid commands and responses are unchanged. Its default
response timeout is five seconds and can expire before the firmware's inactivity
error; do not retry on that stream after a missing `ACK`. Close it and follow the
reset procedure. Error replies may be lost on disconnection. Neither USB timing
nor reconnect behavior has been validated on physical hardware in these tests.
