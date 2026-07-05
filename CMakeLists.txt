cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED PLATFORM)
    set(PLATFORM "host" CACHE STRING "Target platform: host or rp2350")
endif()

message(STATUS "Building for platform: ${PLATFORM}")

if(PLATFORM STREQUAL "rp2350")
    set(PICO_PLATFORM rp2350)
    set(PICO_BOARD waveshare_rp2350_zero)
    set(PICO_FLASH_SIZE_BYTES "(16 * 1024 * 1024)" CACHE STRING
            "Waveshare RP2350-PiZero external flash size" FORCE)

    if(NOT DEFINED PICO_SDK_PATH)
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    endif()

    if(NOT PICO_SDK_PATH)
        message(FATAL_ERROR
            "PICO_SDK_PATH is not set.\n"
            "Set it via environment or CMake flag, for example:\n"
            "export PICO_SDK_PATH=$HOME/pico-sdk\n"
            "cmake -S . -B build-rp2350 -DPLATFORM=rp2350 -DPICO_SDK_PATH=$HOME/pico-sdk\n"
            "Fedora package note: install required toolchain packages first, then clone pico-sdk.")
    endif()

    if(NOT EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
        message(FATAL_ERROR
            "PICO_SDK_PATH is set to '${PICO_SDK_PATH}', but pico_sdk_import.cmake was not found.\n"
            "Expected: ${PICO_SDK_PATH}/external/pico_sdk_import.cmake\n"
            "Ensure the Pico SDK path is correct and initialized.")
    endif()

    message(STATUS "Using Pico SDK at: ${PICO_SDK_PATH}")
    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
endif()

project(mi23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(BUILD_TESTING "Build unit tests" ON)

if(PLATFORM STREQUAL "rp2350")
    pico_sdk_init()
endif()

add_subdirectory(firmware)

if(PLATFORM STREQUAL "host" AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(tests)
endif()
