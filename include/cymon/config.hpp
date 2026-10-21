// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic
//
/// @file config.hpp
/// @brief Compile-time limits of cymon-lib.
///
/// Every limit can be overridden with a preprocessor define (CMake cache
/// variables of the same name, see the README), before any cymon header is
/// included. All storage is static: the limits size the Device object.

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

/// Size of the capture buffer in bytes, split across the channels of one
/// capture (kMaxBufferBytes / sizeof(float) samples in total).
inline constexpr std::size_t kMaxBufferBytes = CYMON_MAX_BUFFER_BYTES;

/// Number of variables the registry holds (every signal the firmware offers).
inline constexpr std::size_t kMaxVars = CYMON_MAX_VARS;

/// Number of variables one capture records at the same time (<= kMaxVars).
inline constexpr std::size_t kMaxChannels = CYMON_MAX_CHANNELS;

/// Longest variable name in bytes (cymon.GetVariableList.0.1).
inline constexpr std::size_t kMaxNameLength = 32U;

/// Longest unit string in bytes (cymon.GetVariableList.0.1).
inline constexpr std::size_t kMaxUnitLength = 8U;

/// Samples (not frames) one ReadSamples response carries (cymon.ReadSamples.0.1).
inline constexpr std::size_t kMaxReadSamples = 64U;

/// Channel IDs one SetupCapture request can carry (cymon.SetupCapture.0.1).
inline constexpr std::size_t kMaxRequestChannels = 16U;

static_assert(kMaxVars > 0U && kMaxVars <= 255U, "CYMON_MAX_VARS must be 1..255 (variable IDs are uint8)");
static_assert(kMaxChannels > 0U && kMaxChannels <= kMaxVars, "CYMON_MAX_CHANNELS must be 1..CYMON_MAX_VARS");
static_assert(kMaxChannels <= kMaxRequestChannels, "CYMON_MAX_CHANNELS exceeds cymon.SetupCapture.0.1");
static_assert(kMaxChannels <= kMaxReadSamples, "one frame must fit into a ReadSamples response");
static_assert(kMaxBufferBytes >= sizeof(float) * kMaxChannels, "CYMON_MAX_BUFFER_BYTES too small");
static_assert(kMaxBufferBytes / sizeof(float) <= 65535U, "frame indices are uint16");

}  // namespace cymon
