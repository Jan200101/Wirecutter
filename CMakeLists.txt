cmake_minimum_required(VERSION 3.15)

set(PICO_BOARD "pico_w")
set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/deps/tinyusb)

include(pico_sdk_import.cmake)

project(btinput)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

pico_sdk_init()

add_subdirectory(src)
