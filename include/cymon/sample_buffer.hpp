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
#include <cstdint>

#include "cymon/config.hpp"

namespace cymon {

/// Fixed-size circular sample buffer for pre/post-trigger capture.
/// Total storage is kMaxBufferBytes bytes = kMaxBufferBytes/4 float32 slots.
class SampleBuffer {
 public:
  static constexpr std::size_t kCapacityBytes = kMaxBufferBytes;
  static constexpr std::size_t kCapacityFloats = kCapacityBytes / sizeof(float);

  /// Configure channels, samples, pretrigger depth.
  /// Returns false if configuration exceeds buffer capacity.
  bool Setup(uint8_t num_channels, uint16_t num_samples,
             uint16_t pretrigger_samples);

  void Arm();
  void Disarm();

  /// Write one frame (num_channels values). Returns false if not armed or
  /// complete.
  bool WriteFrame(const float* values);

  /// Signal trigger. trigger_pos_ = current write_pos_. Returns false if not
  /// armed.
  bool Trigger();

  /// Read frames starting at frame_offset (0 = first pre-trigger frame).
  /// Returns number of frames actually placed in out[].
  uint16_t ReadFrames(uint16_t frame_offset, uint8_t max_frames,
                      float* out) const;

  bool IsComplete() const;
  bool IsArmed() const;
  bool WasTriggered() const;
  uint8_t num_channels() const;
  uint16_t num_samples() const;
  uint16_t capacity_frames() const;

 private:
  float data_[kCapacityFloats] = {};
  uint8_t num_channels_ = 0;
  uint16_t num_samples_ = 0;
  uint16_t pretrigger_samples_ = 0;
  uint16_t capacity_ = 0;
  uint16_t write_pos_ = 0;
  uint16_t trigger_pos_ = 0;
  uint16_t post_trigger_count_ = 0;
  uint16_t total_written_ = 0;
  bool configured_ = false;
  bool armed_ = false;
  bool triggered_ = false;
  bool complete_ = false;
};

}  // namespace cymon
