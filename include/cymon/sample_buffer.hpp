// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic
//
/// @file sample_buffer.hpp
/// @brief Ring buffer of sample frames with pre- and post-trigger depth.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include "cymon/config.hpp"

namespace cymon {

/**
 * @brief Fixed-size ring of frames (one float per channel) for pre/post-trigger capture.
 *
 * Before the trigger the ring runs continuously; after Trigger() it records
 * num_samples - pretrigger_samples further frames and is complete. Frames are
 * read back in time order, frame pretrigger_samples being the trigger sample.
 * Allocation-free; WriteFrame() and Trigger() are safe in an interrupt.
 */
class SampleBuffer {
 public:
  static constexpr std::size_t kCapacityFloats = kMaxBufferBytes / sizeof(float);

  /**
   * @brief Sets the frame layout and disarms the buffer.
   * @return false if the layout is empty or does not fit (the old one stays).
   */
  [[nodiscard]] bool Setup(uint8_t num_channels, uint16_t num_samples, uint16_t pretrigger_samples) noexcept;

  /// Clears the ring and starts recording (requires a successful Setup()).
  void Arm() noexcept;

  void Disarm() noexcept;

  /// Appends one frame of NumChannels() values. Returns false if not armed or complete.
  bool WriteFrame(std::span<const float> values) noexcept;

  /// Marks the next written frame as the trigger sample. Returns false if not armed or already triggered.
  bool Trigger() noexcept;

  /**
   * @brief Copies frames in time order into @p out (interleaved, NumChannels() floats per frame).
   * @param frame_offset First frame (0 = oldest pre-trigger frame).
   * @param max_frames Frames to copy at most; also limited by the size of @p out.
   * @return Number of frames copied. Pre-trigger frames that were never written read as 0.
   */
  [[nodiscard]] uint16_t ReadFrames(uint16_t frame_offset,
                                    uint16_t max_frames,
                                    std::span<float> out) const noexcept;

  [[nodiscard]] bool IsComplete() const noexcept;
  [[nodiscard]] bool IsArmed() const noexcept;
  [[nodiscard]] bool WasTriggered() const noexcept;
  /// True once the pre-trigger part of the ring holds recorded frames only.
  [[nodiscard]] bool PretriggerFilled() const noexcept;
  [[nodiscard]] uint8_t NumChannels() const noexcept;
  [[nodiscard]] uint16_t NumSamples() const noexcept;
  [[nodiscard]] uint16_t PretriggerSamples() const noexcept;
  /// Frames the ring holds with the current channel count.
  [[nodiscard]] uint16_t CapacityFrames() const noexcept;

 private:
  void CopyFrame(uint16_t ring_pos, std::span<float> dest) const noexcept;

  std::array<float, kCapacityFloats> data_{};
  uint8_t num_channels_{0U};
  uint16_t num_samples_{0U};
  uint16_t pretrigger_samples_{0U};
  uint16_t capacity_{0U};
  uint16_t write_pos_{0U};
  uint16_t trigger_pos_{0U};
  uint16_t post_trigger_count_{0U};
  /// Frames written since Arm(), saturating at the pre-trigger depth.
  uint16_t written_before_trigger_{0U};
  bool configured_{false};
  bool armed_{false};
  bool triggered_{false};
  bool complete_{false};
};

}  // namespace cymon
