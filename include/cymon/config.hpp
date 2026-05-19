// Copyright 2026 Tecnologic
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include <cstddef>

#ifndef CYMON_MAX_BUFFER_BYTES
#define CYMON_MAX_BUFFER_BYTES 4096U
#endif
#ifndef CYMON_MAX_VARS
#define CYMON_MAX_VARS 64U
#endif

namespace cymon {

inline constexpr std::size_t kMaxBufferBytes = CYMON_MAX_BUFFER_BYTES;
inline constexpr std::size_t kMaxVars = CYMON_MAX_VARS;
inline constexpr std::size_t kMaxChannels = 16U;
inline constexpr std::size_t kMaxReadFrames = 4U;

}  // namespace cymon
