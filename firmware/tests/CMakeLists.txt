cmake_minimum_required(VERSION 3.14)

if(NOT PLATFORM STREQUAL "host")
    return()
endif()

find_package(GTest REQUIRED)
include(GoogleTest)
enable_testing()

add_executable(mi23_tests
        test_expression.cpp
)

target_include_directories(mi23_tests
        PRIVATE
        ${PROJECT_SOURCE_DIR}/firmware
)

target_link_libraries(mi23_tests
        PRIVATE
        GTest::GTest
        GTest::Main
        expression_lib
)

gtest_discover_tests(mi23_tests)