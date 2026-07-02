set(RP2350_PLATFORM_SOURCE_DIR ${PROJECT_SOURCE_DIR}/firmware/platform/rp2350)
set(ILI9488_SOURCE_DIR ${PROJECT_SOURCE_DIR}/firmware/drivers/ili9488)

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
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_app.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_renderer.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/settings/settings_app.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/settings/settings_state.cpp
        ${PROJECT_SOURCE_DIR}/firmware/hal/settings_store.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/settings_store_rp2350.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/startup_rp2350.cpp
        ${PROJECT_SOURCE_DIR}/firmware/app/home/calculator_home.h
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_app.h
        ${PROJECT_SOURCE_DIR}/firmware/app/graphing/graph_renderer.h
        ${PROJECT_SOURCE_DIR}/firmware/app/settings/settings_app.h
        ${PROJECT_SOURCE_DIR}/firmware/app/settings/settings_state.h
        ${PROJECT_SOURCE_DIR}/firmware/hal/settings_store.h
        ${RP2350_PLATFORM_SOURCE_DIR}/settings_store_rp2350.h
        ${RP2350_PLATFORM_SOURCE_DIR}/startup_rp2350.h
)

target_link_libraries(mi23 PRIVATE
        firmware_headers
        expression_lib
        pico_stdlib
        hardware_spi
        hardware_flash
)

target_include_directories(mi23 PRIVATE
        ${RP2350_PLATFORM_SOURCE_DIR}
)

pico_add_extra_outputs(mi23)
