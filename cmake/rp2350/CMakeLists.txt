set(RP2350_PLATFORM_SOURCE_DIR ${PROJECT_SOURCE_DIR}/firmware/platform/rp2350)
set(ST7789_SOURCE_DIR ${PROJECT_SOURCE_DIR}/firmware/drivers/st7789)

add_executable(mi23
        ${RP2350_PLATFORM_SOURCE_DIR}/main_rp2350.cpp
        ${ST7789_SOURCE_DIR}/st7789.cpp
        ${ST7789_SOURCE_DIR}/st7789.h
        ${RP2350_PLATFORM_SOURCE_DIR}/display_rp2350.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/display_rp2350.h
        ${RP2350_PLATFORM_SOURCE_DIR}/keypad_rp2350.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/keypad_rp2350.h
        ${RP2350_PLATFORM_SOURCE_DIR}/keypad_rp2350_2.cpp
        ${RP2350_PLATFORM_SOURCE_DIR}/keypad_rp2350_2.h
)

target_link_libraries(mi23 PRIVATE
        firmware_headers
        expression_lib
        pico_stdlib
        hardware_spi
)

target_include_directories(mi23 PRIVATE
        ${RP2350_PLATFORM_SOURCE_DIR}
)

pico_add_extra_outputs(mi23)
