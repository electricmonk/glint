# SPDX-License-Identifier: AGPL-3.0-or-later
add_library(glint_dsp STATIC
    GlintEngine.cpp
)
target_include_directories(glint_dsp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(glint_dsp PUBLIC cxx_std_20)
target_compile_options(glint_dsp PRIVATE -Wall -Wextra -Wno-unused-parameter)
set_target_properties(glint_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
