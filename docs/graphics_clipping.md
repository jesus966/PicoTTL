# Graphics clipping and arithmetic safety

`Graphics` and `Text` render onto a `DisplaySurface` independently of the
video backend. Drawing outside the surface is clipped. This contract covers
the full range of their signed coordinate types, including `INT32_MIN` and
`INT32_MAX`, and applies to the framebuffer formats used by MDA, CGA and EGA.

## Coordinate ranges and invariants

Graphics coordinates, lengths and rectangle dimensions are `int32_t`.
Text cursor positions are `int32_t` cell coordinates. Surface dimensions,
bitmap dimensions and font metrics are `uint16_t`.

- Pixel coordinates are zero-based; a surface of width W and height H has
  visible positions in `[0, W)` and `[0, H)`.
- Nonpositive line lengths, rectangle dimensions and row-copy counts do
  nothing. Empty bitmaps and empty surfaces produce no pixels.
- Graphics operations on an unbound surface are no-ops.
- Clipping precedes narrowing to the surface's uint16 coordinate type.
  Signed values are not converted to unsigned coordinates to implement clipping.
- Pixel loops use int32 bounds restricted to the visible surface. Their
  work does not depend on the distance of an offscreen coordinate from it.

The public declarations are in [Graphics.hpp](../include/picottl/Graphics.hpp)
and [Text.hpp](../include/picottl/Text.hpp). Surface ownership and rendering
layers are described in [the architecture guide](architecture.md).

## Runs, rectangles and bitmaps

Horizontal and vertical runs extend rightwards and downwards respectively.
Rectangle inversion covers `[x, x + width)` by `[y, y + height)`; the right
and bottom edges are exclusive. Inversion operates on stored color bits,
with the framebuffer applying its format's value mask.

The interval helper checks that length is positive, evaluates `start + length`
in int64, and intersects the result with the surface before drawing. A
positive length keeps the lower endpoint within int32 range; the surface
bounds the upper endpoint before narrowing. The loops themselves stay in
int32. This allows an extreme positive extent to cover the visible part of
a rectangle without overflowing or traversing its invisible area.

Bitmap blits clip the destination before accessing source rows. Source
offsets are calculated only when the bitmap intersects the surface. With
uint16 bitmap dimensions, `left - x`, `top - y` and their subsequent offsets
are at most 65534. Row byte offsets use `size_t`.

Transparent blits write only set source bits. Opaque blits also paint clear
bits with the background color. Clipping preserves the source bit positions
and does not shift the image into view.

## Row copying

`copyRows(srcY, dstY, rowCount)` preserves the source-to-destination offset
while trimming rows outside either region. Overlapping copies retain their
source contents through the surface's overlap-safe copying implementation.

The arithmetic remains int32. After checking that rowCount is positive,
`-rowCount` is representable. If the smaller start is at or below
`-rowCount`, the copy is wholly clipped and returns before negating that
start. Otherwise, the trim is representable. Checking both starts against
`surface height - trim` before adding the trim prevents overflow of the
other coordinate. Final starts and count fit the uint16 surface API.

## Line rasterization

Lines include both endpoints. Axis-aligned lines use the run primitives.
General lines use symmetric Bresenham rasterization, including its tie
behaviour for each endpoint order.

Clipping retains the supplied endpoints and skips invisible major-axis
steps. Replacing endpoints with geometric intersections would change the
rounding phase and could change visible pixels. Only steps intersecting
the surface's major-axis window are visited: at most the surface width
for an X-major line or the surface height for a Y-major line. Minor-axis
positions are checked before drawing.

For major-axis step k, the minor-axis displacement is:

```text
floor((k * minor + floor(major / 2)) / major)
```

The deltas are formed with widened subtraction and fit uint32. A uint64
phase calculation selects the first potentially visible step; its product
fits uint64 for every `k <= major <= UINT32_MAX`. Division and remainder
are needed only when initial steps are skipped.

The pixel loop uses int32 coordinates and a uint32 remainder. Comparing
the remainder with `major - minor` before adding avoids uint32 overflow,
even for full-range deltas. Coordinates remain between the supplied
endpoints, and the last iteration does not advance them again.

## Text cells and cursor behaviour

Text uses a fixed grid derived from the surface dimensions and font metrics.
Cells are aligned to that grid. A negative column or row is wholly outside
the surface, so it is rejected before multiplication by the glyph dimensions.
For a visible cell, each position product is bounded by the surface size.

Writing an ordinary character advances the column even when its cell is
invisible. A column at or beyond the grid width wraps to column zero on
the next row before rendering. A newline sets column zero and advances
the row; at `INT32_MAX` the row saturates. Subsequent output remains clipped
until the caller moves the cursor. Carriage return sets column zero without
changing the row. If no complete cell fits, ordinary character output is
inert; newline and carriage return still update the cursor.

Glyph backgrounds, fallback cells and underline strokes follow the same
clipping rules. Text scroll checks its line count against the grid before
multiplying: `rows * glyphHeight <= surface height`. Its pixel-row products
therefore fit int32, and any partial-cell band below the grid is untouched.

Terminal clamps its cursor to its grid. Cell counts and indices use `size_t`,
matching its caller-owned buffer-sizing API. Rendering a large logical grid
still requires storage for every cell; safe arithmetic does not remove that
memory requirement.

## Performance and resource requirements

Drawing uses no dynamic allocation, variable-size stack buffers or clipping
tables. Work is bounded by the visible span or area. Lines starting within
the major-axis window avoid the initial 64-bit division; no 64-bit arithmetic
is used in pixel loops. Rendering happens when drawing is requested, outside
the PIO/DMA signal-generation path.

Stack usage depends on the compiler, optimization settings and call chain.
Use `-fstack-usage` with the target toolchain when assessing an application's
stack budget. Host execution time is not a measurement of RP2350 cycle cost.

Font and bitmap descriptors must describe valid caller-owned storage with
sufficient length and stride. Their pointer-based APIs do not carry a storage
byte length. Arithmetic safety does not validate malformed resources or make
concurrent drawing thread-safe.

## Testing the contract

[Graphics tests](../tests/test_graphics.cpp) compare complete framebuffer
contents across coordinate extremes, clipping boundaries, line octants,
endpoint orders, overlapping copies and bitmap transparency. Line checks
include a small-coordinate Bresenham reference and full-range crossing and
offscreen segments. Tests exercise Packed1/2/4/8 formats.

[Text tests](../tests/test_text.cpp) cover extreme cursor positions, wrapping,
fallback and underline rendering, scrolling and empty grids. Host timeouts
guard against nontermination; they do not establish hardware performance.
See [code quality and CI](code_quality.md) for local test and ASan/UBSan
commands. Logical pixel checks and firmware compilation do not establish
physical CRT compatibility.
