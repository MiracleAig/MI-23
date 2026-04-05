find_package(SDL2 REQUIRED)

set(HOST_PLATFORM_SOURCE_DIR ${PROJECT_SOURCE_DIR}/firmware/platform/host)

add_library(host_platform STATIC
        ${HOST_PLATFORM_SOURCE_DIR}/display_sdl.cpp
        ${HOST_PLATFORM_SOURCE_DIR}/display_sdl.h
        ${HOST_PLATFORM_SOURCE_DIR}/keypad_host.cpp
        ${HOST_PLATFORM_SOURCE_DIR}/keypad_host.h
)

target_link_libraries(host_platform PUBLIC
        firmware_headers
)

target_include_directories(host_platform PUBLIC
        ${SDL2_INCLUDE_DIRS}
)

add_subdirectory(sdl_simulator)
