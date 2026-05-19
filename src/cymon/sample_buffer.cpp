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

#include "cymon/sample_buffer.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace cymon {

bool SampleBuffer::Setup(uint8_t num_channels, uint16_t num_samples, uint16_t pretrigger_samples) {
  if (num_channels == 0) {
    return false;
  }
  if (num_channels > kMaxChannels) {
    return false;
  }
  if (pretrigger_samples > num_samples) {
    return false;
  }

  const uint16_t cap = static_cast<uint16_t>(kCapacityFloats / num_channels);
  if (num_samples > cap) {
    return false;
  }
  if (num_samples == 0) {
    return false;
  }

  num_channels_ = num_channels;
  num_samples_ = num_samples;
  pretrigger_samples_ = pretrigger_samples;
  capacity_ = cap;
  configured_ = true;
  armed_ = false;
  triggered_ = false;
  complete_ = false;
  write_pos_ = 0;
  trigger_pos_ = 0;
  post_trigger_count_ = 0;
  total_written_ = 0;
  return true;
}

void SampleBuffer::Arm() {
  write_pos_ = 0;
  trigger_pos_ = 0;
  post_trigger_count_ = 0;
  total_written_ = 0;
  written_before_trigger_ = 0;
  armed_ = true;
  triggered_ = false;
  complete_ = false;
}

void SampleBuffer::Disarm() {
  armed_ = false;
  triggered_ = false;
  complete_ = false;
}

bool SampleBuffer::WriteFrame(const float* values) {
  if (!armed_ || complete_) {
    return false;
  }

  const std::size_t offset = static_cast<std::size_t>(write_pos_) * num_channels_;
  for (uint8_t i = 0; i < num_channels_; ++i) {
    data_[offset + i] = values[i];
  }

  write_pos_ = static_cast<uint16_t>((write_pos_ + 1U) % capacity_);

  if (total_written_ < std::numeric_limits<uint16_t>::max()) {
    ++total_written_;
  }

  if (triggered_) {
    ++post_trigger_count_;
    const uint16_t post_needed = static_cast<uint16_t>(num_samples_ - pretrigger_samples_);
    if (post_trigger_count_ >= post_needed) {
      complete_ = true;
    }
  }

  return true;
}

bool SampleBuffer::Trigger() {
  if (!armed_ || triggered_) {
    return false;
  }

  triggered_ = true;
  trigger_pos_ = write_pos_;
  written_before_trigger_ = total_written_;
  post_trigger_count_ = 0;

  if (pretrigger_samples_ >= num_samples_) {
    complete_ = true;
  }

  return true;
}

uint16_t SampleBuffer::ReadFrames(uint16_t frame_offset, uint8_t max_frames, float* out) const {
  if (!triggered_) {
    return 0;
  }

  uint16_t n = 0;
  for (uint16_t i = 0; i < max_frames; ++i) {
    const uint16_t abs_k = static_cast<uint16_t>(frame_offset + i);
    if (abs_k >= num_samples_) {
      break;
    }

    float* dest = out + static_cast<std::size_t>(n) * num_channels_;

    if (abs_k < pretrigger_samples_) {
      // Pre-trigger frame: check if enough data was written before trigger.
      const uint16_t frames_before = static_cast<uint16_t>(pretrigger_samples_ - abs_k);
      if (written_before_trigger_ < frames_before) {
        // Not enough pre-trigger data; fill zeros.
        for (uint8_t c = 0; c < num_channels_; ++c) {
          dest[c] = 0.0F;
        }
      } else {
        const uint16_t ring_pos =
            static_cast<uint16_t>((static_cast<uint32_t>(trigger_pos_) + capacity_ - pretrigger_samples_ + abs_k) % capacity_);
        const std::size_t src_offset = static_cast<std::size_t>(ring_pos) * num_channels_;
        for (uint8_t c = 0; c < num_channels_; ++c) {
          dest[c] = data_[src_offset + c];
        }
      }
    } else {
      // Post-trigger frame.
      const uint16_t post_idx = static_cast<uint16_t>(abs_k - pretrigger_samples_);
      if (post_idx >= post_trigger_count_) {
        break;
      }

      const uint16_t ring_pos = static_cast<uint16_t>((static_cast<uint32_t>(trigger_pos_) + post_idx) % capacity_);
      const std::size_t src_offset = static_cast<std::size_t>(ring_pos) * num_channels_;
      for (uint8_t c = 0; c < num_channels_; ++c) {
        dest[c] = data_[src_offset + c];
      }
    }
    ++n;
  }
  return n;
}

bool SampleBuffer::IsComplete() const { return complete_; }
bool SampleBuffer::IsArmed() const { return armed_; }
bool SampleBuffer::WasTriggered() const { return triggered_; }
uint8_t SampleBuffer::num_channels() const { return num_channels_; }
uint16_t SampleBuffer::num_samples() const { return num_samples_; }
uint16_t SampleBuffer::capacity_frames() const { return capacity_; }

}  // namespace cymon
