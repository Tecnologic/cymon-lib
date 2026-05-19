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

# generate_cyphal_types(target dsdl_dir)
# Generates C++ headers from DSDL files using nunavut and adds them to target.
function(generate_cyphal_types target dsdl_dir)
  find_program(NUNAVUT_EXECUTABLE nunavut
    HINTS "$ENV{HOME}/.local/bin" "/usr/local/bin"
    DOC "nunavut DSDL code generation tool"
  )

  if(NOT NUNAVUT_EXECUTABLE)
    message(FATAL_ERROR
      "nunavut not found. Install it with: pip install nunavut\n"
      "Then re-run cmake."
    )
  endif()

  set(generated_dir "${CMAKE_BINARY_DIR}/generated")
  file(MAKE_DIRECTORY "${generated_dir}")

  file(GLOB_RECURSE dsdl_files "${dsdl_dir}/*.dsdl")

  add_custom_command(
    OUTPUT "${generated_dir}/.nunavut_stamp"
    COMMAND "${NUNAVUT_EXECUTABLE}"
      --target-language c++
      --outdir "${generated_dir}"
      "${dsdl_dir}"
    COMMAND "${CMAKE_COMMAND}" -E touch "${generated_dir}/.nunavut_stamp"
    DEPENDS ${dsdl_files}
    COMMENT "Generating Cyphal C++ types from DSDL"
    VERBATIM
  )

  add_custom_target(nunavut_generate
    DEPENDS "${generated_dir}/.nunavut_stamp"
  )

  add_dependencies(${target} nunavut_generate)
  target_include_directories(${target} PUBLIC "${generated_dir}")
endfunction()
