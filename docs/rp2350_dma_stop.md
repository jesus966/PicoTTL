# RP2350 DMA stop: audit finding A2

## Evidence and scope

The [RP2350 datasheet](https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf),
section 12.6.8.3 and erratum RP2350-E5 (D.4.1), requires clearing EN and
disabling CHAIN_TO on all channels before aborting the group. It recommends
simultaneously aborting channels involved in a chain. E5 affects silicon
revisions A2, A3 and A4 in the datasheet consulted for this change.

Aborting can fire a completion chain and an enabled channel can accept a
trigger at the end of abort. An already-issued bus write cannot be revoked.
Stopping the PIO state machines does not establish that DMA has retired:
FIFO space, DREQ credits and outstanding bus writes can still exist.

In the project's Pico SDK 2.3.0:

- [`dma_channel_abort()`](https://github.com/raspberrypi/pico-sdk/blob/2.3.0/src/rp2_common/hardware_dma/include/hardware/dma.h)
  writes one abort bit and waits for BUSY to clear. Its documentation requires
  the caller to clear EN for the aborted channel and chained channels. It
  does not perform that prerequisite itself.
- [`dma_channel_cleanup()`](https://github.com/raspberrypi/pico-sdk/blob/2.3.0/src/rp2_common/hardware_dma/dma.c)
  clears EN/CHAIN_TO for one channel, disables its IRQs, aborts it and clears
  raw IRQ status. Calling it sequentially is not a substitute for first
  disabling the whole chained group.

A2 is therefore a confirmed missing documented prerequisite, rather than
an SDK workaround already applied on PicoTTL's behalf. Actual occurrence
of a post-abort restart on a board has not been reproduced or measured here.
The previous chain break and control-first abort order already reduced
restart paths in this particular topology. Missing the documented workaround
is not evidence that every stop, or a deterministic host sequence, fails.

## Resource relationships and sequencing

Channel numbers are caller-configurable; defaults are:

| Path | Data | Control | Trigger relationships |
|---|---:|---:|---|
| Timing | 0 | 1 | data CHAIN_TO control; control writes data AL3_READ_ADDR_TRIG |
| Pixels | 2 | 3 | data CHAIN_TO control; control writes data AL3_READ_ADDR_TRIG |

Control channels have CHAIN_TO=self. Pixel channels are optional but remain
owned once acquired, even after the pixel source is detached. The engine
does not register interrupt handlers or enable DMA interrupts. Completion
can nevertheless latch raw INTR bits.

Previously, stop disabled PIO, broke only data-channel chains, then aborted
timing control/data followed by pixel control/data. EN remained set throughout.

Now stop:

1. Disables all owned PIO state machines.
2. Builds the DMA mask from resource ownership, including retained pixel resources.
3. Uses the non-triggering AL1_CTRL alias to clear EN and set CHAIN_TO=self
   on **every** owned channel before the first abort.
4. Writes the complete mask to CHAN_ABORT once, stopping both members of
   both loops simultaneously. Control writes already in flight may finish,
   but their target data channels cannot accept a start with EN cleared.
5. Waits for the owned ABORT bits and BUSY flags to clear, then clears only
   those channels' raw interrupt status, empties owned FIFOs and parks outputs
   if the engine had been running.
6. Clears the running flag. Repeated stop, stop before the first start and
   destruction after partial configuration quiesce acquired resources too.
   Unacquired resources are never touched.

The blocking retirement wait retains the SDK's assumption of functioning
bus slaves: outstanding transfers normally finish within a few cycles and
do not require further PIO DREQ. It adds no fixed delay or FIFO-drain loop.
A wedged bus slave could still block, as with the existing SDK abort. A
timeout followed by resource release would be unsafe; this change does not
add a new failure-return API or claim recovery from a hardware bus lockup.

Start reapplies the existing DMA configurations before triggering either
loop, restoring EN and CHAIN_TO after safe stop. The API, PIO programs,
timing words, clock divisors, signal polarity and synchronized SM enable
sequence are unchanged. No additional channels or state machines are used.

### Relationship to M1 and concurrent callers

The old direct stop/start path already failed to restore data-channel
CHAIN_TO (M1). Clearing EN for E5 additionally makes explicit rearming
necessary. Reusing setupDma/setupPixelDma in start is that required rearm;
restoring the original chain follows from the same configuration. There
is no independent M1 redesign, new restart API or hardware validation of M1.

Lifecycle operations must remain serialized by their caller. There are no
engine ISRs that rearm channels. Concurrent start/configure/stop from another
core or an external ISR writing the same registers is unsupported; this
change does not introduce locks or claim thread safety. Other DMA channels
must not chain into, or write trigger/configuration registers of, owned channels.

## Automated validation boundaries

The standalone host suite executes the same private stop-order helper used
in production, with callbacks recording disable, group abort and retirement
wait operations. Tests exercise active timing-only and chained pixel paths,
pending control-write preconditions, repeated stop/release preconditions,
reconfiguration preconditions, retained pixel ownership after detach, partial
initialization, non-default channel numbers, unowned channels, empty groups
and already-retired groups.

These are ordering/logic tests, not an emulation of silicon. The recorder
supplies register-operation outcomes; it does not prove the hardware callback
implementation, actual late-write behaviour or full PioScanlineEngine
start/configure execution. Existing host device lifecycle tests exercise real
MDA/CGA/EGA device code with a recording scanline engine, not the real PIO/DMA
engine. Cross-compilation checks the production implementation against SDK 2.3.0.
The existing CI static-analysis/coverage scope excludes the hardware engine;
the private helper receives an additional direct host analyzer check locally.

Validation for this change: 77 host cases (68 existing plus 9 stop-order cases)
pass both natively and with ASan/UBSan; 30 quality infrastructure tests pass.
The Pico 2 Release build produces all 51 UF2 targets with SDK 2.3.0 and Arm
GCC 15.2.1. clang-tidy 18.1.3 and cppcheck 2.13.0 retain the 9 and 3 known
diagnostic occurrences respectively, with no new or missing baseline entries.
Direct analysis of the new host test/helper adds no own-code diagnostics.
CodeQL CLI 2.27.3 host analysis produces no alerts. actionlint 1.7.12 and
`git diff --check` pass. Host-only coverage remains 83.29% lines and 85.34%
functions under the existing scope; those percentages do not measure PIO/DMA.

## Physical validation procedure (not executed)

Use a separate diagnostic firmware for Pico 2 with the same engine and SDK;
do not add logging to the real-time PIO or modify existing examples. Record
board silicon revision, system clock, SDK/toolchain revisions and mode/depth.

1. Reserve a spare GPIO as a scope marker: set it immediately before stop,
   clear it on return. Capture HSYNC, VSYNC, video and the marker with a logic
   analyzer/oscilloscope. Keep USB/printf outside the marked interval; store
   register snapshots in RAM and print afterwards. Use SWD if a stop hangs.
2. Snapshot each owned channel's CTRL_TRIG (EN, BUSY, CHAIN_TO, READ_ERROR,
   WRITE_ERROR), READ_ADDR, WRITE_ADDR, TRANS_COUNT, and the group CHAN_ABORT,
   INTR and INTE0..3. Also record PIO CTRL, FLEVEL, FSTAT, FDEBUG and SM PCs.
   Capture before stop, immediately after return and repeatedly while stopped
   for at least several frame periods. Reading these registers must not write
   aliases or otherwise trigger DMA. Reset sticky debug flags only between trials.
3. On return require EN=0, BUSY=0, CHAIN_TO=self and ABORT=0 for every owned
   channel, both SM enable bits clear, FIFOs empty and outputs at idle polarity.
   DMA addresses/counts must then stay unchanged until an explicit restart.
   Raw IRQ status must remain clear unless deliberately changed externally.
   A delayed BUSY/EN transition, address/count movement or renewed FIFO writes
   while stopped is a failure. Do not require TRANS_COUNT=0 after a late
   control write: quiescence and no further movement are the criteria.
4. Run at least 100,000 iterations at varying frame/scanline phases. Include
   direct configure/start/stop, stop/stop, stop/destruct/reacquire, and
   stop/configure/start. Arrange pauses close to data completion/control
   reload; use isolated background DMA traffic to increase bus contention
   without sharing the engine's channels or writing their registers.
5. Cover timing only, connected framebuffer, stop/detach/reconfigure/start,
   and all supported MDA/CGA/EGA depths/geometries. Exercise safe configuration
   failure (e.g. pre-claim pixel resources so acquisition is rejected) and
   destruction afterwards; ensure another owner can claim all released resources.
   Never concurrently mutate the framebuffer or lifecycle from another core.
6. Repeat with the correct TTL adapter and compatible CRT for each standard.
   Measure active-video HSYNC/VSYNC periods, widths, polarities and pixel
   alignment against the unmodified build. Confirm stable picture after
   restart and no DMA activity while stopped. Stopping intentionally removes
   raster sync; the CRT may lose lock and take time to reacquire it. Check
   recovery separately from the DMA assertions; a visible picture alone does
   not establish E5 correctness.

Passing the automated tests and producing UF2 files does not replace these
measurements or establish CRT compatibility.
