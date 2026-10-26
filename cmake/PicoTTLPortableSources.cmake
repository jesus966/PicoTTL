# Canonical portable production sources, shared by firmware and host builds.
# Absolute paths keep inclusion independent of the consuming project directory.
set(PICOTTL_PORTABLE_SOURCES
    "${CMAKE_CURRENT_LIST_DIR}/../src/AnsiTerminal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/DisplaySurface.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/Graphics.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/Terminal.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/Text.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmCga8x8.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmCga8x8Cp850.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmEga8x14.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmEga8x14Cp850.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmMda9x14.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/fonts/IbmMda9x14Cp850.cpp"
)
