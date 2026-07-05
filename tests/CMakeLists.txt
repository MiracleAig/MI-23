if(NOT PLATFORM STREQUAL "host")
    return()
endif()

find_package(GTest REQUIRED)
include(GoogleTest)
enable_testing()

add_executable(mi23_tests
        test_boot_manager.cpp
        test_calculator_app.cpp
        test_display_primitives.cpp
        test_expression.cpp
        test_axiom_fs.cpp
        test_file_browser.cpp
        test_graph_storage.cpp
        test_graph_renderer.cpp
        test_home_screen.cpp
        test_math_typeset.cpp
        test_settings_app.cpp
        test_settings_store.cpp
        test_system_status.cpp
        ../firmware/app/calculator/calculator_app.cpp
        ../firmware/app/boot/boot_manager.cpp
        ../firmware/app/files/file_browser_app.cpp
        ../firmware/app/graphing/graph_app.cpp
        ../firmware/app/graphing/graph_renderer.cpp
        ../firmware/app/graphing/graph_storage.cpp
        ../firmware/app/home/calculator_home.cpp
        ../firmware/app/settings/settings_app.cpp
        ../firmware/app/settings/settings_state.cpp
        ../firmware/app/ui/system_status.cpp
        ../firmware/app/ui/title_bar.cpp
        ../firmware/hal/settings_store.cpp
        ../firmware/platform/host/simulator_keypad.cpp
        ../firmware/platform/host/settings_store_host.cpp
        ../firmware/platform/host/startup_host.cpp
)

target_include_directories(mi23_tests
        PRIVATE
        ../firmware
)

target_link_libraries(mi23_tests
        PRIVATE
        GTest::GTest
        GTest::Main
        axiom_fs
        expression_lib
)

gtest_discover_tests(mi23_tests)
