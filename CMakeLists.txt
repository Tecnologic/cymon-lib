# Copyright 2026 Tecnologic
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 4.0.0)
project(cymon-lib VERSION 0.1.0 LANGUAGES CXX)

# Library
add_library(cymon
  src/cymon/trigger.cpp
  src/cymon/sample_buffer.cpp
  src/cymon/device.cpp
)
target_compile_features(cymon PUBLIC cxx_std_23)
target_include_directories(cymon PUBLIC include)
target_compile_options(cymon PRIVATE -Wall -Wextra -Wpedantic)

# Optional nunavut DSDL code generation
option(CYMON_GENERATE_DSDL "Generate C++ types from DSDL via nunavut" OFF)
if(CYMON_GENERATE_DSDL)
  include(cmake/nunavut.cmake)
  generate_cyphal_types(cymon ${CMAKE_CURRENT_SOURCE_DIR}/dsdl)
endif()

# Tests
option(CYMON_BUILD_TESTS "Build tests" OFF)
if(CYMON_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
