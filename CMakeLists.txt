# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Tecnologic
#
# cymon-lib: device side of the cymon trace protocol (C++20, no heap, no
# Cyphal dependency). Built standalone (host tests) or with add_subdirectory()
# from a firmware build such as the UNIMOC Zephyr module, which then applies
# its own toolchain flags; this file adds warnings only when it is the top
# level project.

cmake_minimum_required(VERSION 3.20)
project(cymon-lib VERSION 0.2.0 LANGUAGES CXX)

set(CYMON_MAX_BUFFER_BYTES 4096 CACHE STRING "Capture buffer size in bytes")
set(CYMON_MAX_VARS 64 CACHE STRING "Variables the registry holds (1..255)")
set(CYMON_MAX_CHANNELS 8 CACHE STRING "Channels of one capture (1..16)")

add_library(cymon STATIC
  src/cymon/trigger.cpp
  src/cymon/sample_buffer.cpp
  src/cymon/device.cpp
)
add_library(cymon::cymon ALIAS cymon)
target_compile_features(cymon PUBLIC cxx_std_20)
target_include_directories(cymon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(cymon PUBLIC
  CYMON_MAX_BUFFER_BYTES=${CYMON_MAX_BUFFER_BYTES}U
  CYMON_MAX_VARS=${CYMON_MAX_VARS}U
  CYMON_MAX_CHANNELS=${CYMON_MAX_CHANNELS}U
)

# Directory of the DSDL root namespace for the firmware's Nunavut generation.
set(CYMON_DSDL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/dsdl/cymon CACHE INTERNAL "cymon DSDL root namespace")

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_CXX_EXTENSIONS OFF)
  # The warning set of UNIMOC, so the library builds clean inside it.
  target_compile_options(cymon PRIVATE
    -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wold-style-cast -Wcast-align -Wunused
    -Woverloaded-virtual -Wconversion -Wsign-conversion -Wnull-dereference -Wdouble-promotion
    -Wformat=2 -Werror)
endif()

option(CYMON_BUILD_TESTS "Build the GoogleTest host tests" ${PROJECT_IS_TOP_LEVEL})
if(CYMON_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
