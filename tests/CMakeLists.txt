# SPDX-License-Identifier: AGPL-3.0-or-later
include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(glint_tests
    test_engine.cpp
    test_grains.cpp
    test_reverb.cpp
    test_shimmer.cpp
)
target_link_libraries(glint_tests PRIVATE glint_dsp Catch2::Catch2WithMain)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(glint_tests)
