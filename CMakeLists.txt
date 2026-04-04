cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED PLATFORM)
    set(PLATFORM "host" CACHE STRING "Target platform: host or rp2350")
endif()

message(STATUS "Building for platform: ${PLATFORM}")

if(PLATFORM STREQUAL "rp2350")
    set(PICO_PLATFORM rp2350)
    set(PICO_BOARD waveshare_rp2350_zero)

    if(NOT DEFINED PICO_SDK_PATH)
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    endif()

    if(NOT PICO_SDK_PATH)
        message(FATAL_ERROR "PICO_SDK_PATH is not set. Pass it with -DPICO_SDK_PATH=/home/miracleaigbogun/pico-sdk")
    endif()

    message(STATUS "Using Pico SDK at: ${PICO_SDK_PATH}")
    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
endif()

project(mi23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(PLATFORM STREQUAL "rp2350")
    pico_sdk_init()
endif()

add_subdirectory(firmware)

if(PLATFORM STREQUAL "host")
    add_subdirectory(tests)
endif()
