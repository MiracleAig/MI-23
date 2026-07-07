add_library(firmware_headers INTERFACE)
target_include_directories(firmware_headers INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(math)

add_library(axiom_fs STATIC
        hal/fs/axiom_fs.cpp
        hal/fs/axiom_fs.h
        hal/fs/fs_logger.cpp
        hal/fs/fs_logger.h
)

target_link_libraries(axiom_fs PUBLIC
        firmware_headers
)

if(PLATFORM STREQUAL "host")
    target_sources(axiom_fs PRIVATE
            platform/host/axiom_fs_host.cpp
            platform/host/axiom_fs_host.h
    )

    add_subdirectory(
            ${PROJECT_SOURCE_DIR}/cmake/host
            ${CMAKE_CURRENT_BINARY_DIR}/platform/host
    )

    add_library(calculator_app STATIC
            app/boot/boot_manager.cpp
            app/boot/boot_manager.h
            app/calculator/calculator_app.cpp
            app/calculator/calculator_app.h
            app/calculator/history.h
            app/companion/companion_link_app.cpp
            app/companion/companion_link_app.h
            app/files/file_browser_app.cpp
            app/files/file_browser_app.h
            app/graphing/graph_app.cpp
            app/graphing/graph_app.h
            app/graphing/graph_renderer.cpp
            app/graphing/graph_renderer.h
            app/graphing/graph_storage.cpp
            app/graphing/graph_storage.h
            app/settings/settings_app.cpp
            app/settings/settings_app.h
            app/settings/settings_state.cpp
            app/settings/settings_state.h
            app/ui/system_status.cpp
            app/ui/system_status.h
            app/ui/title_bar.cpp
            app/ui/title_bar.h
            hal/settings_store.cpp
            hal/settings_store.h
            graphics/font.h
            hal/display.h
            hal/keypad.h
            hal/usb_cdc_transport.h
            core/companion/CompanionProtocol.cpp
            core/companion/CompanionProtocol.h
            core/companion/CompanionJson.cpp
            core/companion/CompanionJson.h
            core/companion/CompanionSystemActions.h
            core/companion/CompanionSession.cpp
            core/companion/CompanionSession.h
            core/companion/CompanionUtils.cpp
            core/companion/CompanionUtils.h
            platform/host/settings_store_host.cpp
            platform/host/settings_store_host.h
            platform/host/startup_host.cpp
            platform/host/startup_host.h
            app/home/calculator_home.cpp
            app/home/calculator_home.h
    )

    target_link_libraries(calculator_app
            PUBLIC
            firmware_headers
            axiom_fs
            host_platform
            PRIVATE
            expression_lib
    )
    target_compile_definitions(calculator_app PRIVATE
            MI23_ENABLE_DEVELOPER_OPTIONS=${MI23_DEVELOPER_OPTIONS_DEFINE}
    )
elseif(PLATFORM STREQUAL "rp2350")
    target_sources(axiom_fs PRIVATE
            platform/rp2350/axiom_fs_flash_block_device.cpp
            platform/rp2350/axiom_fs_flash_block_device.h
            platform/rp2350/axiom_fs_flash_config.h
            platform/rp2350/axiom_fs_rp2350.cpp
            platform/rp2350/axiom_fs_rp2350.h
    )
    target_link_libraries(axiom_fs PUBLIC
            pico_stdlib
            hardware_flash
    )

    add_subdirectory(
            ${PROJECT_SOURCE_DIR}/cmake/rp2350
            ${CMAKE_CURRENT_BINARY_DIR}/platform/rp2350
    )
else()
    message(FATAL_ERROR "Unsupported PLATFORM '${PLATFORM}'. Expected 'host' or 'rp2350'.")
endif()
