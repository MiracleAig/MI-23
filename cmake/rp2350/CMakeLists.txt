set(RP2350_PLATFORM_SOURCE_DIR ${PROJECT_SOURCE_DIR}/firmware/platform/rp2350)
set(ILI9488_SOURCE_DIR ${PROJECT_SOURCE_DIR}/firmware/drivers/ili9488)

set(MI23_FIRMWARE_VERSION "dev" CACHE STRING "MI-23 firmware version label")
set(MI23_ENABLE_LITTLEFS ON CACHE BOOL "Build the RP2350 AxiomFS backend with LittleFS" FORCE)
if(NOT MI23_LITTLEFS_SOURCE_DIR)
    set(MI23_LITTLEFS_SOURCE_DIR "${PROJECT_SOURCE_DIR}/third_party/littlefs"
            CACHE PATH "Directory containing lfs.h, lfs.c, and lfs_util.c" FORCE)
else()
    set(MI23_LITTLEFS_SOURCE_DIR "${MI23_LITTLEFS_SOURCE_DIR}"
            CACHE PATH "Directory containing lfs.h, lfs.c, and lfs_util.c")
endif()

add_executable(mi23
        ${PROJECT_SOURCE_DIR}/firmware/app/boot/boot_manager.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/boot/boot_manager.h
        ${RP2350_PLATFORM_SOURCE_DIR}/main_rp2350.cpp
        ${ILI9488_SOURCE_DIR}/ili9488.cpp
        ${ILI9488_SOURCE_DIR}/ili9488.h
        ${RP2350_PLATFORM_SOURCE_DIR}/display_rp2350.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/display_rp2350.h
        ${RP2350_PLATFORM_SOURCE_DIR}/keypad_rp2350.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/keypad_rp2350.h
        ${RP2350_PLATFORM_SOURCE_DIR}/keypad_rp2350_2.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/keypad_rp2350_2.h
        ${PROJECT_SOURCE_DIR}/firmware/app/home/calculator_home.cpp
        ${CMAKE_SOURCE_DIR}/firmware/app/calculator/calculator_app.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/files/file_browser_app.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_app.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_renderer.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_storage.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/settings/settings_app.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/settings/settings_state.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/ui/system_status.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/ui/title_bar.cpp
        ${PROJECT_SOURCE_DIR}/firmware/hal/settings_store.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/settings_store_rp2350.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/startup_rp2350.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/home/calculator_home.h
        ${PROJECT_SOURCE_DIR}/firmware/app/files/file_browser_app.h
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_app.h
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_renderer.h
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_storage.h
        ${PROJECT_SOURCE_DIR}/firmware/app/settings/settings_app.h
        ${PROJECT_SOURCE_DIR}/firmware/app/settings/settings_state.h
        ${PROJECT_SOURCE_DIR}/firmware/app/ui/system_status.h
        ${PROJECT_SOURCE_DIR}/firmware/app/ui/title_bar.h
        ${PROJECT_SOURCE_DIR}/firmware/hal/settings_store.h
        ${RP2350_PLATFORM_SOURCE_DIR}/settings_store_rp2350.h
        ${RP2350_PLATFORM_SOURCE_DIR}/startup_rp2350.h
)

target_link_libraries(mi23 PRIVATE
        firmware_headers
        axiom_fs
        expression_lib
        pico_stdlib
        hardware_spi
        hardware_flash
)

target_include_directories(mi23 PRIVATE
        ${RP2350_PLATFORM_SOURCE_DIR}
)

target_compile_definitions(mi23 PRIVATE
        MI23_BUILD_TARGET="rp2350"
        MI23_FIRMWARE_VERSION="${MI23_FIRMWARE_VERSION}"
)

pico_enable_stdio_usb(mi23 1)
pico_enable_stdio_uart(mi23 0)

if(MI23_ENABLE_LITTLEFS)
    if(NOT MI23_LITTLEFS_SOURCE_DIR)
        message(FATAL_ERROR "MI23_ENABLE_LITTLEFS is ON, but MI23_LITTLEFS_SOURCE_DIR is not set.")
    endif()
    if(NOT EXISTS "${MI23_LITTLEFS_SOURCE_DIR}/lfs.h"
            OR NOT EXISTS "${MI23_LITTLEFS_SOURCE_DIR}/lfs.c"
            OR NOT EXISTS "${MI23_LITTLEFS_SOURCE_DIR}/lfs_util.c")
        message(FATAL_ERROR
                "MI23_LITTLEFS_SOURCE_DIR must contain lfs.h, lfs.c, and lfs_util.c.")
    endif()

    target_sources(axiom_fs PRIVATE
            ${MI23_LITTLEFS_SOURCE_DIR}/lfs.c
            ${MI23_LITTLEFS_SOURCE_DIR}/lfs_util.c
    )
    target_include_directories(axiom_fs PUBLIC
            ${MI23_LITTLEFS_SOURCE_DIR}
    )
    target_compile_definitions(axiom_fs PUBLIC
            MI23_ENABLE_LITTLEFS=1
    )
else()
    target_compile_definitions(axiom_fs PUBLIC
            MI23_ENABLE_LITTLEFS=0
    )
endif()

pico_add_extra_outputs(mi23)
