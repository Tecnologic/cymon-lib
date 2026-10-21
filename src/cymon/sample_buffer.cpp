// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic
//
/// @file sample_buffer.cpp
/// @brief Ring buffer of sample frames with pre- and post-trigger depth.

#include "cymon/sample_buffer.hpp"
#include <algorithm>

namespace cymon {

bool SampleBuffer::Setup(uint8_t num_channels, uint16_t num_samples, uint16_t pretrigger_samples) noexcept {
  if (num_channels == 0U || num_channels > kMaxChannels) {
    return false;
  }
  const auto kCapacity = static_cast<uint16_t>(kCapacityFloats / num_channels);
  if (num_samples == 0U || num_samples > kCapacity || pretrigger_samples >= num_samples) {
    return false;
  }

  num_channels_ = num_channels;
  num_samples_ = num_samples;
  pretrigger_samples_ = pretrigger_samples;
  capacity_ = kCapacity;
  configured_ = true;
  Disarm();
  return true;
}

void SampleBuffer::Arm() noexcept {
  if (!configured_) {
    return;
  }
  write_pos_ = 0U;
  trigger_pos_ = 0U;
  post_trigger_count_ = 0U;
  written_before_trigger_ = 0U;
  triggered_ = false;
  complete_ = false;
  armed_ = true;
}

void SampleBuffer::Disarm() noexcept {
  armed_ = false;
  triggered_ = false;
  complete_ = false;
}

bool SampleBuffer::WriteFrame(std::span<const float> values) noexcept {
  if (!armed_ || complete_ || values.size() < num_channels_) {
    return false;
  }

  const std::size_t kOffset = static_cast<std::size_t>(write_pos_) * num_channels_;
  std::copy_n(values.begin(), num_channels_, data_.begin() + static_cast<std::ptrdiff_t>(kOffset));
  write_pos_ = static_cast<uint16_t>((write_pos_ + 1U) % capacity_);

  if (triggered_) {
    ++post_trigger_count_;
    if (post_trigger_count_ >= num_samples_ - pretrigger_samples_) {
      complete_ = true;
    }
  } else if (written_before_trigger_ < pretrigger_samples_) {
    ++written_before_trigger_;
  }
  return true;
}

bool SampleBuffer::Trigger() noexcept {
  if (!armed_ || triggered_) {
    return false;
  }
  triggered_ = true;
  trigger_pos_ = write_pos_;
  post_trigger_count_ = 0U;
  return true;
}

void SampleBuffer::CopyFrame(uint16_t ring_pos, std::span<float> dest) const noexcept {
  const std::size_t kOffset = static_cast<std::size_t>(ring_pos) * num_channels_;
  std::copy_n(data_.begin() + static_cast<std::ptrdiff_t>(kOffset), num_channels_, dest.begin());
}

uint16_t SampleBuffer::ReadFrames(uint16_t frame_offset,
                                  uint16_t max_frames,
                                  std::span<float> out) const noexcept {
  if (!triggered_ || num_channels_ == 0U) {
    return 0U;
  }
  const std::size_t kFit = out.size() / num_channels_;
  const auto kLimit = static_cast<uint16_t>(std::min<std::size_t>(max_frames, kFit));

  uint16_t copied = 0U;
  for (; copied < kLimit; ++copied) {
    const auto kFrame = static_cast<uint32_t>(frame_offset) + copied;
    if (kFrame >= num_samples_) {
      break;
    }
    const std::span<float> kDest =
        out.subspan(static_cast<std::size_t>(copied) * num_channels_, num_channels_);

    if (kFrame < pretrigger_samples_) {
      // The ring holds written_before_trigger_ frames directly before the trigger sample.
      const uint32_t kAge = pretrigger_samples_ - kFrame;
      if (kAge > written_before_trigger_) {
        std::fill(kDest.begin(), kDest.end(), 0.0F);
      } else {
        CopyFrame(static_cast<uint16_t>((trigger_pos_ + capacity_ - kAge) % capacity_), kDest);
      }
    } else {
      const uint32_t kPostIndex = kFrame - pretrigger_samples_;
      if (kPostIndex >= post_trigger_count_) {
        break;
      }
      CopyFrame(static_cast<uint16_t>((trigger_pos_ + kPostIndex) % capacity_), kDest);
    }
  }
  return copied;
}

bool SampleBuffer::IsComplete() const noexcept { return complete_; }
bool SampleBuffer::IsArmed() const noexcept { return armed_; }
bool SampleBuffer::WasTriggered() const noexcept { return triggered_; }
bool SampleBuffer::PretriggerFilled() const noexcept {
  return written_before_trigger_ >= pretrigger_samples_;
}
uint8_t SampleBuffer::NumChannels() const noexcept { return num_channels_; }
uint16_t SampleBuffer::NumSamples() const noexcept { return num_samples_; }
uint16_t SampleBuffer::PretriggerSamples() const noexcept { return pretrigger_samples_; }
uint16_t SampleBuffer::CapacityFrames() const noexcept { return capacity_; }

}  // namespace cymon
