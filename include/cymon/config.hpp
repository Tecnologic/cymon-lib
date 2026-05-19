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
#ifndef CYMON_MAX_CHANNELS
#define CYMON_MAX_CHANNELS 8U
#endif

namespace cymon {

/// Total size of the capture buffer in bytes. The buffer holds
/// kMaxBufferBytes / sizeof(float) float32 slots split across all active
/// channels. Override with -DCYMON_MAX_BUFFER_BYTES=<bytes>.
inline constexpr std::size_t kMaxBufferBytes = CYMON_MAX_BUFFER_BYTES;

/// Maximum number of named scalar variables that can be registered on the
/// device (the full variable registry, e.g. every signal the firmware exposes).
/// Hard upper bound is 255 (IDs are uint8). Override with -DCYMON_MAX_VARS=<count>.
inline constexpr std::size_t kMaxVars = CYMON_MAX_VARS;
static_assert(kMaxVars <= 255U, "CYMON_MAX_VARS must not exceed 255 (variable IDs are uint8)");

/// Maximum number of variables that can be captured *simultaneously* in a
/// single capture session. Always <= kMaxVars. A monitor selects up to
/// kMaxChannels variables from the registry for one capture run.
/// Hard upper bound is 255. Override with -DCYMON_MAX_CHANNELS=<count>.
inline constexpr std::size_t kMaxChannels = CYMON_MAX_CHANNELS;
static_assert(kMaxChannels <= 255U, "CYMON_MAX_CHANNELS must not exceed 255 (channel IDs are uint8)");

/// Maximum number of sample frames returned in a single ReadSamples response.
inline constexpr std::size_t kMaxReadFrames = 4U;

}  // namespace cymon
