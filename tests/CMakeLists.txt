# Host unit tests for src/app (pure logic, no HAL).
# Built with the normal PC compiler, separately from the firmware:
#
#   cmake -S tests -B build-tests
#   cmake --build build-tests
#   ctest --test-dir build-tests --output-on-failure

cmake_minimum_required(VERSION 3.22)
project(f3scope_tests CXX)

# tests run on the PC: refuse to build them with the ARM cross compiler
if(CMAKE_CROSSCOMPILING OR CMAKE_CXX_COMPILER MATCHES "arm-none-eabi")
    message(FATAL_ERROR
        "Unit tests must be built with the host compiler, not ${CMAKE_CXX_COMPILER}.\n"
        "Use a separate build directory:  cmake -S tests -B build-tests\n"
        "and do not add_subdirectory(tests) from the firmware CMakeLists.txt.")
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 17)              # same as the firmware
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # for clangd

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../src)

# --- GoogleTest ----------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- Code under test: the hardware-independent app layer ---------------------
add_library(app STATIC
    ${SRC_DIR}/app/trigger.cpp
    ${SRC_DIR}/app/commands.cpp
    ${SRC_DIR}/app/protocol.cpp
    ${SRC_DIR}/app/wavegen.cpp
    ${SRC_DIR}/app/timer_math.cpp
    ${SRC_DIR}/app/measure.cpp
)
target_include_directories(app PUBLIC ${SRC_DIR})   # "app/trigger.hpp"

# --- Warnings and runtime checks (for app and tests, not for GoogleTest) -----
add_library(test_flags INTERFACE)
target_compile_options(test_flags INTERFACE
    -Wall -Wextra -Wshadow
    $<$<CONFIG:Debug>:-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer>
)
target_link_options(test_flags INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)
# bounds checks in the standard library: string_view[i] out of range aborts
target_compile_definitions(test_flags INTERFACE _GLIBCXX_ASSERTIONS)
target_link_libraries(app PUBLIC test_flags)

# --- Tests: every tests/test_*.cpp is picked up automatically ---------------
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)
if(NOT TEST_SOURCES)
    message(FATAL_ERROR "no tests/test_*.cpp files yet")
endif()

add_executable(unit_tests ${TEST_SOURCES})
target_link_libraries(unit_tests PRIVATE app GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(unit_tests)
