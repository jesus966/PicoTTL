# Integrating PicoTTL

PicoTTL can be included directly in an existing Raspberry Pi Pico SDK
project. The consuming project owns SDK initialization, its executable
targets and all board-level configuration.

## Requirements

- Raspberry Pi Pico 2 or another RP2350 board
- Raspberry Pi Pico SDK 2.3.0 (the validated version)
- CMake 3.13 or later
- C++17
- Working native SDK host tools (pioasm and, for UF2 output, picotool), or a
  native compiler/dependencies able to build them. On Windows, an unrelated
  `cl.exe` on PATH is not necessarily a working MSVC toolchain. A consuming
  project may need its installed SDK `pioasm_DIR`/`picotool_DIR` package paths;
  PicoTTL's top-level VS Code helper is not run when added as a subdirectory.

PicoTTL must be added after `pico_sdk_init()`. Adding it earlier produces a
configuration error rather than a later failure inside the SDK.

## Minimal project

A minimal consuming project has this layout:

```text
my-project/
  CMakeLists.txt
  pico_sdk_import.cmake
  main.cpp
  external/
    PicoTTL/
```

Its `CMakeLists.txt` follows the normal Pico SDK initialization order:

```cmake
cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico2 CACHE STRING "Board type")

include(pico_sdk_import.cmake)

project(my_project C CXX ASM)

pico_sdk_init()

add_subdirectory(external/PicoTTL)

add_executable(my_app
    main.cpp
)

target_link_libraries(my_app PRIVATE
    picottl
    pico_stdlib
)

pico_add_extra_outputs(my_app)
```

A minimal `main.cpp` can start an MDA sync-only display:

```cpp
#include "hardware/clocks.h"
#include "pico/stdlib.h"

#include "picottl/PicoTTL.hpp"
#include "picottl/rp2350/MdaDisplayDevice.hpp"

int main() {
    set_sys_clock_khz(130'000, true);

    picottl::rp2350::MdaDisplayDevice::Config config;
    config.hsyncPin = 2;

    static picottl::rp2350::MdaDisplayDevice device(config);
    picottl::Display display(device);
    if (!display.begin(picottl::VideoMode::IbmMda)) {
        return 1;
    }

    while (true) {
        tight_loop_contents();
    }
}
```

The static backend keeps its embedded timing table off the main stack; its
local initialization still follows clock setup. This is sync-only output,
not text or pixels. Verify the monitor/interface before connecting hardware.

The numbered examples contain complete framebuffer, graphics, text and
terminal applications. Their CMake targets are not added to a consuming
project.

## Adding the source

### Git submodule

```sh
git submodule add https://github.com/jesus966/PicoTTL.git external/PicoTTL
git submodule update --init --recursive
```

Use `add_subdirectory(external/PicoTTL)` as shown above.

### Vendored copy

Copy the PicoTTL source tree into `external/PicoTTL` and use the same
`add_subdirectory()` call. Keep `CMakeLists.txt`, `cmake/`, `include/`, `src/` and
`pico_sdk_import.cmake`; the root build imports its canonical portable list
from `cmake/`. Generated PIO headers are produced by the build.

### FetchContent

`FetchContent_MakeAvailable()` requires CMake 3.14 or later:

```cmake
include(FetchContent)

FetchContent_Declare(
    picottl_source
    GIT_REPOSITORY https://github.com/jesus966/PicoTTL.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(picottl_source)

target_link_libraries(my_app PRIVATE picottl)
```

Declare and make PicoTTL available after `pico_sdk_init()`.

## Build contract

The `picottl` target and public includes such as
`picottl/PicoTTL.hpp` are stable interfaces. Internal directory names,
CMake variables and targets used by PicoTTL's own examples, diagnostics,
tests and tools are not public interfaces.

PicoTTL requires an RP2350 target. The maintainer has physically tested all
examples on a Raspberry Pi Pico 2 using Arm Cortex-M33, with an IBM 5151
and a Samtron SC-431E. These results do not establish physical validation
of RISC-V execution or imply unrecorded electrical measurements.

## Host tests

The host tests deliberately use a separate native-toolchain build and do
not become part of the consuming firmware project:

```sh
cmake -S external/PicoTTL/tests -B build-picottl-host -G Ninja
cmake --build build-picottl-host
ctest --test-dir build-picottl-host --output-on-failure --verbose --no-tests=error
```