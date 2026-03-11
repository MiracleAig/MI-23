cmake_minimum_required(VERSION 3.16)
project(mi23)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED PLATFORM)
    set(PLATFORM "host")
endif()

message(STATUS "Building for platform: ${PLATFORM}")

if(PLATFORM STREQUAL "host")
    find_package(SDL2 REQUIRED)

    add_executable(mi23
            firmware/main_host.cpp
            firmware/hal/host/display_sdl.cpp
            firmware/hal/host/keypad_host.cpp
    )

    target_include_directories(mi23 PRIVATE
            firmware/
            ${SDL2_INCLUDE_DIRS}
    )

    # This is the fixed line — SDL2::SDL2 is the correct modern target name
    target_link_libraries(mi23 PRIVATE SDL2::SDL2)
endif()