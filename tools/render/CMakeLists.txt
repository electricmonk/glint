# SPDX-License-Identifier: AGPL-3.0-or-later
add_executable(glint-render main.cpp)
target_link_libraries(glint-render PRIVATE glint_dsp)
