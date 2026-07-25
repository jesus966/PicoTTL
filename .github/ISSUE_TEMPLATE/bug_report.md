---
name: Bug report
about: Something does not work as documented
title: ""
labels: bug
assignees: ""
---

## Summary

A clear, one-paragraph description of the problem.

## Environment

- PicoTTL version / commit:
- Board: (e.g. Raspberry Pi Pico 2, RP2350)
- Pico SDK version:
- Toolchain: (ARM GCC version, CMake, Ninja)
- Host OS:

## Hardware setup (if signal-related)

- Monitor make/model: (e.g. IBM 5151, Samtron SC-431E)
- Wiring: reference connector? (docs/architecture.md §15) If not,
  describe the exact GPIO -> DE-9 mapping.
- Series resistors used (values per line):
- System clock (`set_sys_clock_khz` value):
- Power source:

## Steps to reproduce

1. Example/app and mode used (e.g. `44_code_pages`, `IbmMda`):
2. Exact steps:

## Expected behaviour

What the example header / documentation says should happen.

## Actual behaviour

What happens instead. For visual problems, a photo of the CRT is worth
a thousand words; scope traces of HSYNC/VSYNC are welcome for sync
issues.

## Host test suite

Output of `./build-host/picottl_tests` (pass/fail count), if you can
run it.

## Additional context

Anything else: does it reproduce on the host backend? Did it work on a
previous commit?
