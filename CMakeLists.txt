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

add_library(expression_lib STATIC
        firmware/core/expression.cpp
        firmware/core/expression.h
)
target_include_directories(expression_lib PUBLIC
        firmware/
)

if(PLATFORM STREQUAL "host")
    find_package(SDL2 REQUIRED)

    add_executable(mi23
            firmware/main_host.cpp
            firmware/hal/host/display_sdl.cpp
            firmware/hal/host/keypad_host.cpp
            firmware/graphics/font.h
            firmware/ui/history.h
            firmware/ui/calculator_app.cpp
            firmware/ui/calculator_app.h
            firmware/hal/rp2350/config/pin_config.h
    )

    target_include_directories(mi23 PRIVATE
            firmware/
            ${SDL2_INCLUDE_DIRS}
    )

    target_link_libraries(mi23 PRIVATE
            SDL2::SDL2
            expression_lib
    )

    find_package(GTest REQUIRED)
    include(GoogleTest)
    enable_testing()

    add_executable(mi23_tests
            firmware/tests/test_expression.cpp
    )

    target_include_directories(mi23_tests PRIVATE
            firmware/
    )

    target_link_libraries(mi23_tests PRIVATE
            GTest::GTest
            GTest::Main
            expression_lib
    )

    gtest_discover_tests(mi23_tests)

elseif(PLATFORM STREQUAL "rp2350")

    add_executable(mi23
            firmware/hal/rp2350/main_rp2350.cpp
            firmware/hal/rp2350/drivers/st7789.cpp
            firmware/hal/rp2350/drivers/st7789.h
            firmware/hal/rp2350/hal/display_rp2350.cpp
            firmware/hal/rp2350/hal/display_rp2350.h
            firmware/hal/rp2350/hal/keypad_rp2350.cpp
            firmware/hal/rp2350/hal/keypad_rp2350.h
            firmware/hal/rp2350/hal/keypad_rp2350_2.cpp
            firmware/hal/rp2350/hal/keypad_rp2350_2.h
    )

    target_link_libraries(mi23 PRIVATE
            pico_stdlib
            hardware_spi
            expression_lib
    )

    target_include_directories(mi23 PRIVATE
            firmware/
            firmware/hal/rp2350/
    )

    pico_add_extra_outputs(mi23)

endif()