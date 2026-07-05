add_executable(mi23
        ${PROJECT_SOURCE_DIR}/firmware/platform/host/main_host.cpp
)

target_link_libraries(mi23 PRIVATE
        calculator_app
        host_platform
)

if(WIN32 AND TARGET SDL2::SDL2main)
    target_link_libraries(mi23 PRIVATE
            SDL2::SDL2main
    )
    target_link_options(mi23 PRIVATE
            -static-libgcc
            -static-libstdc++
    )
endif()

if(BUILD_RELEASE AND TARGET SDL2::SDL2-static)
    target_link_libraries(mi23 PRIVATE
            SDL2::SDL2-static
            pthread
            dl
    )
    message(STATUS "SDL2: static linking (release build)")
elseif(TARGET SDL2::SDL2)
    target_link_libraries(mi23 PRIVATE
            SDL2::SDL2
    )
    if(BUILD_RELEASE)
        message(STATUS "SDL2: static target unavailable; dynamic linking (release build)")
    else()
        message(STATUS "SDL2: dynamic linking (debug build)")
    endif()
else()
    target_link_libraries(mi23 PRIVATE
            ${SDL2_LIBRARIES}
    )
    message(STATUS "SDL2: dynamic linking via SDL2_LIBRARIES")
endif()
