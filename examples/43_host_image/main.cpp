// PicoTTL - Example 43: host image display
//
// THE RECOMMENDED STARTING POINT FOR HOST-DRIVEN GRAPHICS: a complete
// end-to-end demonstration that a PC can generate graphics and display
// them on a real MDA/CGA/EGA monitor without writing any firmware.
//
//   PC image -> conversion (host) -> framebuffer bytes -> USB CDC
//            -> PicoTTL framebuffer -> real monitor
//
// The firmware is deliberately tiny: it initializes the selected
// PicoTTL backend, receives complete framebuffers over USB CDC, copies
// the bytes into the framebuffer, and acknowledges. No decoding, no
// scaling, no color conversion, no compression, no analysis: the Pico
// remains ONLY a display device. Everything intelligent happens in the
// companion Python application (see host/).
//
// This example is NOT a prototype of PicoTTL Stream. It is the
// pedagogical minimum. PicoTTL Stream builds on exactly these concepts
// and adds rectangles, compression, delta encoding, adaptive updates
// and video - see its protocol specification.
//
// Protocol (USB CDC, any baud rate; single-letter commands):
//   'H'                     -> one-line HELLO with the mode table
//   'M' + u8 modeIndex      -> switch mode; reply "OK <fbBytes>" / "ERR ..."
//   'F' + u32le length      -> then exactly <length> raw framebuffer
//      + framebuffer bytes     bytes (must equal the active mode's
//                              size); reply "ACK" / "ERR ..."
// Bytes are written straight into the live framebuffer as they arrive
// (the image paints top-down on the monitor - that is the demo).
//
// Modes served by ONE firmware. Following PicoTTL's canonical-
// connector philosophy, the CGA modes are driven by EgaDisplayDevice
// in its CGA-compatible timings - exactly how IBM's EGA card served
// CGA monitors - so every mode works on the reference connector. The
// host converter emits EGA 200-line pixel values for the CGA modes
// (intensity on Secondary Green = DE-9 pin 6). Two backends, one PIO
// block each, disjoint DMA channels (claims-are-sticky rule):
//   0  IbmMda     720x350 Packed2   63,000 B  (MDA monitor)
//   1  IbmCga640  640x200 Packed8  128,000 B  (CGA monitor, EGA device)
//   2  IbmCga320  320x200 Packed8   64,000 B  (CGA monitor, EGA device)
//   3  IbmEga350  640x350 Packed8  224,000 B  (EGA monitor)
//
// Wiring (PicoTTL reference connector - EGA superset; syncs shared):
//   GPIO 2  -> HSYNC (DE-9 pin 8)     GPIO 3 -> VSYNC (DE-9 pin 9)
//   GPIO 4  -> internal handshake, LEAVE UNCONNECTED
//   GPIO 16 -> DE-9 pin 5 (Blue / Primary Blue)
//   GPIO 17 -> DE-9 pin 4 (Green / Primary Green)
//   GPIO 18 -> DE-9 pin 3 (Red / Primary Red)
//   GPIO 19 -> DE-9 pin 7 (MDA VIDEO / Secondary Blue)
//   GPIO 20 -> DE-9 pin 6 (MDA INTENSITY / CGA Intensity / Sec. Green)
//   GPIO 21 -> DE-9 pin 2 (Secondary Red; GND inside MDA/CGA monitors)
//   GND     -> GND (DE-9 pin 1)
//   470 ohm series resistors on HSYNC and VSYNC only; connect all other
//   monitor signal wires directly. GND goes to DE-9 pin 1 only.
//
// Expected: the selected monitor shows a stable raster in the active
// mode; running `python host/main.py --port COMx --mode <name> <image>`
// paints the converted image top-down on the screen, and 'M' commands
// switch modes live (including MDA <-> CGA <-> EGA reclocking).
//
// A failure here (with the per-backend examples passing) indicates a
// regression in the multi-device PIO/DMA partitioning, the mode-switch
// reclock path, or the CDC protocol handling - not in the display path.
//
// Copyright (c) 2026 Jesús Fernández Gamito
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstdio>

#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/rp2350/EgaDisplayDevice.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

namespace {

using namespace picottl;

// ---------------------------------------------------------------------------
// One storage block sized for the largest mode; one correctly-sized
// framebuffer view per mode. All views share the same bytes, so the
// received image always lands at the start of gStorage.
// ---------------------------------------------------------------------------

constexpr std::size_t kStorageBytes =
    PackedFramebuffer8::bytesFor(modeWidth(VideoMode::IbmEga350),
                                 modeHeight(VideoMode::IbmEga350));

alignas(std::uint32_t) std::uint8_t gStorage[kStorageBytes];

PackedFramebuffer2 gFbMda{gStorage, sizeof gStorage,
                          modeWidth(VideoMode::IbmMda),
                          modeHeight(VideoMode::IbmMda)};
PackedFramebuffer8 gFbCga640{gStorage, sizeof gStorage,
                             modeWidth(VideoMode::IbmCga640),
                             modeHeight(VideoMode::IbmCga640)};
PackedFramebuffer8 gFbCga320{gStorage, sizeof gStorage,
                             modeWidth(VideoMode::IbmCga320),
                             modeHeight(VideoMode::IbmCga320)};
PackedFramebuffer8 gFbEga{gStorage, sizeof gStorage,
                          modeWidth(VideoMode::IbmEga350),
                          modeHeight(VideoMode::IbmEga350)};

// ---------------------------------------------------------------------------
// The two backends, one per PIO block with disjoint DMA channels so
// this single firmware can switch between them at runtime (hardware
// claims are sticky per device; distinct resources avoid collisions).
// ---------------------------------------------------------------------------

rp2350::MdaDisplayDevice::Config mdaConfig() {
    rp2350::MdaDisplayDevice::Config c;
    c.pioInstance = 0;
    c.dmaDataChannel = 0;
    c.dmaControlChannel = 1;
    c.pixelDmaDataChannel = 2;
    c.pixelDmaControlChannel = 3;
    c.hsyncPin = 2; // VSYNC on GPIO 3
    c.pixelPins[0] = 19; // VIDEO
    c.pixelPins[1] = 20; // INTENSITY
    c.framebuffer = &gFbMda;
    return c;
}

rp2350::EgaDisplayDevice::Config egaConfig() {
    rp2350::EgaDisplayDevice::Config c;
    c.pioInstance = 1;
    c.dmaDataChannel = 4;
    c.dmaControlChannel = 5;
    c.pixelDmaDataChannel = 6;
    c.pixelDmaControlChannel = 7;
    c.hsyncPin = 2;
    c.vsyncPin = 3;
    c.primaryBluePin = 16;
    c.primaryGreenPin = 17;
    c.primaryRedPin = 18;
    c.secondaryBluePin = 19;
    c.secondaryGreenPin = 20;
    c.secondaryRedPin = 21;
    c.framebuffer = &gFbEga;
    return c;
}

rp2350::MdaDisplayDevice gMdaDevice{mdaConfig()};
rp2350::EgaDisplayDevice gEgaDevice{egaConfig()};

Display gMdaDisplay{gMdaDevice};
Display gEgaDisplay{gEgaDevice};

struct ModeEntry {
    const char* name;
    VideoMode mode;
    Display* display;
    Framebuffer* framebuffer;
    std::uint32_t clockKhz;
    std::uint32_t fbBytes;
};

const ModeEntry kModes[] = {
    {"mda", VideoMode::IbmMda, &gMdaDisplay, &gFbMda, 130'000,
     PackedFramebuffer2::bytesFor(720, 350)},
    {"cga640", VideoMode::IbmCga640, &gEgaDisplay, &gFbCga640, 142'800,
     PackedFramebuffer8::bytesFor(640, 200)},
    {"cga320", VideoMode::IbmCga320, &gEgaDisplay, &gFbCga320, 142'800,
     PackedFramebuffer8::bytesFor(320, 200)},
    {"ega350", VideoMode::IbmEga350, &gEgaDisplay, &gFbEga, 130'000,
     PackedFramebuffer8::bytesFor(640, 350)},
};
constexpr std::size_t kModeCount = sizeof kModes / sizeof kModes[0];

std::size_t gActiveMode = 0;

/// Switches modes exactly like any PicoTTL application: end the active
/// display, reclock for the target crystal domain, rebind the
/// framebuffer view, begin. USB CDC survives the reclock (it runs from
/// the separate 48 MHz USB PLL).
bool applyMode(std::size_t index) {
    if (index >= kModeCount) {
        return false;
    }
    kModes[gActiveMode].display->end();
    set_sys_clock_khz(kModes[index].clockKhz, true);
    if (!kModes[index].display->setFramebuffer(kModes[index].framebuffer) ||
        !kModes[index].display->begin(kModes[index].mode)) {
        // Restore the previous mode: never leave the monitor rasterless.
        set_sys_clock_khz(kModes[gActiveMode].clockKhz, true);
        kModes[gActiveMode].display->setFramebuffer(
            kModes[gActiveMode].framebuffer);
        kModes[gActiveMode].display->begin(kModes[gActiveMode].mode);
        return false;
    }
    gActiveMode = index;
    return true;
}

int readByteBlocking() {
    while (true) {
        const int c = getchar_timeout_us(100'000);
        if (c >= 0) {
            return c;
        }
    }
}

/// Reads exactly `count` bytes into `out` (blocking, chunked).
void readExact(std::uint8_t* out, std::uint32_t count) {
    while (count > 0) {
        const int c = getchar_timeout_us(100'000);
        if (c < 0) {
            continue;
        }
        *out++ = static_cast<std::uint8_t>(c);
        --count;
    }
}

void handleHello() {
    printf("PICOTTL-HOSTIMAGE V1 ACTIVE %u MODES", (unsigned)gActiveMode);
    for (std::size_t i = 0; i < kModeCount; ++i) {
        printf(" %u:%s:%lu", (unsigned)i, kModes[i].name,
               (unsigned long)kModes[i].fbBytes);
    }
    printf("\n");
}

void handleSetMode() {
    const auto index = static_cast<std::size_t>(readByteBlocking());
    if (applyMode(index)) {
        printf("OK %lu\n", (unsigned long)kModes[gActiveMode].fbBytes);
    } else {
        printf("ERR bad mode\n");
    }
}

void handleFrame() {
    std::uint32_t length = 0;
    for (int i = 0; i < 4; ++i) {
        length |= static_cast<std::uint32_t>(readByteBlocking()) << (8 * i);
    }
    if (length != kModes[gActiveMode].fbBytes || length > sizeof gStorage) {
        // Consume the payload so the stream stays in sync, then report.
        for (std::uint32_t i = 0; i < length && i < sizeof gStorage; ++i) {
            (void)readByteBlocking();
        }
        printf("ERR expected %lu bytes\n",
               (unsigned long)kModes[gActiveMode].fbBytes);
        return;
    }
    // Straight into the live framebuffer: the image paints top-down on
    // the monitor as it arrives. The application owns this storage;
    // PicoTTL keeps scanning it out with zero CPU involvement.
    readExact(gStorage, length);
    printf("ACK\n");
}

} // namespace

int main() {
    set_sys_clock_khz(kModes[0].clockKhz, true);
    stdio_init_all(); // USB CDC (see CMakeLists: USB on, UART off)

    const bool ok = kModes[0].display->begin(kModes[0].mode);
    gActiveMode = 0;

#ifdef PICO_DEFAULT_LED_PIN
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, ok);
#endif

    while (true) {
        switch (readByteBlocking()) {
            case 'H':
                handleHello();
                break;
            case 'M':
                handleSetMode();
                break;
            case 'F':
                handleFrame();
                break;
            default:
                break; // Unknown bytes are ignored.
        }
    }
}
