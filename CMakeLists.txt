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
option(MI23_ENABLE_DEVELOPER_OPTIONS "Enable hidden developer diagnostics menus" OFF)

set(MI23_PRODUCT_ID "mi23" CACHE STRING "MI-23 product identifier used by companion services")
set(MI23_PRODUCT_NAME "MI-23" CACHE STRING "MI-23 product display name")
set(MI23_FIRMWARE_VERSION "0.1.0-alpha.1" CACHE STRING "MI-23 public firmware version")
set(MI23_COMPANION_PROTOCOL_VERSION "1" CACHE STRING "MI-23 companion protocol version")
set(MI23_FILESYSTEM_SCHEMA_VERSION "1" CACHE STRING "MI-23 AxiomFS/LittleFS schema version")

if(PLATFORM STREQUAL "host")
    set(MI23_PLATFORM_ID "host")
    set(MI23_DEFAULT_HARDWARE_REVISION "host-simulator")
    set(MI23_DEFAULT_DEVICE_ID "mi23-host-simulator")
    set(MI23_FLASH_SIZE_BYTES "0")
    set(MI23_FILESYSTEM_OFFSET_BYTES "0")
    set(MI23_FILESYSTEM_SIZE_BYTES "0")
    set(MI23_DEFAULT_SUPPORTS_BOOTSEL_REBOOT OFF)
    set(MI23_DEFAULT_SUPPORTS_FIRMWARE_UPDATE OFF)
elseif(PLATFORM STREQUAL "rp2350")
    set(MI23_PLATFORM_ID "rp2350")
    set(MI23_DEFAULT_HARDWARE_REVISION "waveshare-rp2350-pizero")
    set(MI23_DEFAULT_DEVICE_ID "mi23-rp2350-unknown")
    set(MI23_FLASH_SIZE_BYTES "16777216")
    set(MI23_FILESYSTEM_OFFSET_BYTES "14680064")
    set(MI23_FILESYSTEM_SIZE_BYTES "2097152")
    set(MI23_DEFAULT_SUPPORTS_BOOTSEL_REBOOT ON)
    set(MI23_DEFAULT_SUPPORTS_FIRMWARE_UPDATE ON)
else()
    set(MI23_PLATFORM_ID "${PLATFORM}")
    set(MI23_DEFAULT_HARDWARE_REVISION "unknown")
    set(MI23_DEFAULT_DEVICE_ID "mi23-unknown-device")
    set(MI23_FLASH_SIZE_BYTES "0")
    set(MI23_FILESYSTEM_OFFSET_BYTES "0")
    set(MI23_FILESYSTEM_SIZE_BYTES "0")
    set(MI23_DEFAULT_SUPPORTS_BOOTSEL_REBOOT OFF)
    set(MI23_DEFAULT_SUPPORTS_FIRMWARE_UPDATE OFF)
endif()

if(NOT DEFINED MI23_HARDWARE_REVISION)
    set(MI23_HARDWARE_REVISION "${MI23_DEFAULT_HARDWARE_REVISION}" CACHE STRING
            "MI-23 hardware revision identifier reported to companion services")
else()
    set(MI23_HARDWARE_REVISION "${MI23_HARDWARE_REVISION}" CACHE STRING
            "MI-23 hardware revision identifier reported to companion services")
endif()

if(NOT DEFINED MI23_SUPPORTS_BOOTSEL_REBOOT)
    set(MI23_SUPPORTS_BOOTSEL_REBOOT "${MI23_DEFAULT_SUPPORTS_BOOTSEL_REBOOT}" CACHE BOOL
            "Whether this build can reboot into USB BOOT/BOOTSEL mode")
else()
    set(MI23_SUPPORTS_BOOTSEL_REBOOT "${MI23_SUPPORTS_BOOTSEL_REBOOT}" CACHE BOOL
            "Whether this build can reboot into USB BOOT/BOOTSEL mode")
endif()

if(NOT DEFINED MI23_SUPPORTS_FIRMWARE_UPDATE)
    set(MI23_SUPPORTS_FIRMWARE_UPDATE "${MI23_DEFAULT_SUPPORTS_FIRMWARE_UPDATE}" CACHE BOOL
            "Whether this build is eligible for companion firmware update flows")
else()
    set(MI23_SUPPORTS_FIRMWARE_UPDATE "${MI23_SUPPORTS_FIRMWARE_UPDATE}" CACHE BOOL
            "Whether this build is eligible for companion firmware update flows")
endif()

set(MI23_SUPPORTS_FILE_TRANSFER ON CACHE BOOL
        "Whether this build exposes companion filesystem read/write transfer commands")
set(MI23_SUPPORTS_FILESYSTEM_BACKUP ON CACHE BOOL
        "Whether this build exposes enough filesystem read/list support for companion backups")

if(NOT MI23_COMPANION_PROTOCOL_VERSION MATCHES "^[0-9]+$")
    message(FATAL_ERROR "MI23_COMPANION_PROTOCOL_VERSION must be an integer")
endif()
if(NOT MI23_FILESYSTEM_SCHEMA_VERSION MATCHES "^[0-9]+$")
    message(FATAL_ERROR "MI23_FILESYSTEM_SCHEMA_VERSION must be an integer")
endif()
if(NOT MI23_FIRMWARE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+([-+][0-9A-Za-z.-]+)?$")
    message(FATAL_ERROR "MI23_FIRMWARE_VERSION must be a semantic version string such as 0.1.0-alpha.1")
endif()
foreach(MI23_UINT_NAME
        MI23_FLASH_SIZE_BYTES
        MI23_FILESYSTEM_OFFSET_BYTES
        MI23_FILESYSTEM_SIZE_BYTES)
    if(NOT ${MI23_UINT_NAME} MATCHES "^[0-9]+$")
        message(FATAL_ERROR "${MI23_UINT_NAME} must be an unsigned integer byte count")
    endif()
endforeach()
if(MI23_FILESYSTEM_SIZE_BYTES GREATER 0)
    math(EXPR MI23_FILESYSTEM_END_BYTES
            "${MI23_FILESYSTEM_OFFSET_BYTES} + ${MI23_FILESYSTEM_SIZE_BYTES}")
    if(MI23_FILESYSTEM_END_BYTES GREATER MI23_FLASH_SIZE_BYTES)
        message(FATAL_ERROR "MI23 filesystem region exceeds configured flash size")
    endif()
endif()

foreach(MI23_BOOL_NAME
        MI23_SUPPORTS_BOOTSEL_REBOOT
        MI23_SUPPORTS_FIRMWARE_UPDATE
        MI23_SUPPORTS_FILE_TRANSFER
        MI23_SUPPORTS_FILESYSTEM_BACKUP)
    if(${MI23_BOOL_NAME})
        set(${MI23_BOOL_NAME}_CPP true)
    else()
        set(${MI23_BOOL_NAME}_CPP false)
    endif()
endforeach()

set(MI23_DEVELOPER_OPTIONS_DEFINE 0)
if(MI23_ENABLE_DEVELOPER_OPTIONS)
    set(MI23_DEVELOPER_OPTIONS_DEFINE 1)
endif()

if(PLATFORM STREQUAL "rp2350")
    pico_sdk_init()
endif()

add_subdirectory(firmware)

if(PLATFORM STREQUAL "host" AND BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(tests)
endif()
