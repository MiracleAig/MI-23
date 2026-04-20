add_library(firmware_headers INTERFACE)
target_include_directories(firmware_headers INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(math)

if(PLATFORM STREQUAL "host")
    add_subdirectory(
            ${PROJECT_SOURCE_DIR}/cmake/host
            ${CMAKE_CURRENT_BINARY_DIR}/platform/host
    )

    add_library(calculator_app STATIC
            app/calculator/calculator_app.cpp
            app/calculator/calculator_app.h
            app/calculator/history.h
            graphics/font.h
            hal/display.h
            hal/keypad.h
            app/home/calculator_home.cpp
            app/home/calculator_home.h
    )

    target_link_libraries(calculator_app
            PUBLIC
            firmware_headers
            host_platform
            PRIVATE
            expression_lib
    )
elseif(PLATFORM STREQUAL "rp2350")
    add_subdirectory(
            ${PROJECT_SOURCE_DIR}/cmake/rp2350
            ${CMAKE_CURRENT_BINARY_DIR}/platform/rp2350
    )
else()
    message(FATAL_ERROR "Unsupported PLATFORM '${PLATFORM}'. Expected 'host' or 'rp2350'.")
endif()
