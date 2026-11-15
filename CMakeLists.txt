# SPDX-License-Identifier: AGPL-3.0-or-later
cmake_minimum_required(VERSION 3.25)

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(Glint VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(GLINT_BUILD_TESTS "Build Catch2 tests and the render tool" ON)

# ---------------------------------------------------------------------------
# Pure DSP library: no JUCE allowed in here.
# ---------------------------------------------------------------------------
add_subdirectory(src/dsp)

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)
add_subdirectory(src/plugin)

if(GLINT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tools/render)
    add_subdirectory(tests)
endif()
