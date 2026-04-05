add_executable(mi23
        ${PROJECT_SOURCE_DIR}/firmware/platform/host/main_host.cpp
)

target_link_libraries(mi23 PRIVATE
        calculator_app
        host_platform
)

if(BUILD_RELEASE)
    target_link_libraries(mi23 PRIVATE
            SDL2::SDL2-static
            pthread
            dl
    )
    message(STATUS "SDL2: static linking (release build)")
else()
    target_link_libraries(mi23 PRIVATE
            SDL2::SDL2
    )
    message(STATUS "SDL2: dynamic linking (debug build)")
endif()
