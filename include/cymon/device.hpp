// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic
//
/// @file device.hpp
/// @brief Device side of the cymon trace protocol: variable registry, capture and trigger.

#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <span>
#include <string_view>
#include "cymon/config.hpp"
#include "cymon/sample_buffer.hpp"
#include "cymon/trigger.hpp"

namespace cymon {

/// Result of a request; the values are those of cymon.Error.0.1.
enum class ErrorCode : uint8_t {
  kNone = 0,
  kBadChannelId = 1,      ///< A channel or the trigger source is no registered variable.
  kBufferTooSmall = 2,    ///< num_samples does not fit the buffer with this channel count.
  kNotConfigured = 3,     ///< ArmTrigger before a successful SetupCapture.
  kAlreadyCapturing = 5,  ///< SetupCapture while armed; disarm first.
  kBadConfig = 9,         ///< No or too many channels, pretrigger >= num_samples, bad trigger mode.
};

/// Capture state; the values are those of cymon.CaptureState.0.1.
enum class CaptureState : uint8_t {
  kIdle = 0,        ///< No capture configured.
  kConfigured = 1,  ///< Configured, not armed (also after a disarm).
  kArmed = 2,       ///< Recording the pre-trigger ring, waiting for the trigger.
  kTriggered = 3,   ///< Triggered, recording the post-trigger part.
  kComplete = 4,    ///< Capture complete; the samples can be read.
};

/**
 * @brief Variable registry and oscilloscope-style capture of one node.
 *
 * The library has no Cyphal dependency: the integrating firmware deserializes
 * the cymon service requests (dsdl/cymon), calls the Handle*() methods and
 * serializes their results.
 *
 * Execution contexts:
 * - Tick() runs in the sampling context, typically an interrupt at a fixed
 *   rate. It is allocation-free, lock-free and calls no function pointers.
 * - Everything else runs in one thread (the Cyphal thread). Tick() may
 *   preempt it at any point: the capture state is an atomic that the thread
 *   publishes last, and the thread changes the capture setup only while Tick()
 *   ignores it (not armed). Tick() and the thread must not run in parallel on
 *   two cores.
 */
class Device {
 public:
  struct VariableInfo {
    std::string_view name;
    std::string_view unit;
  };

  struct CaptureConfig {
    std::array<uint8_t, kMaxChannels> channel_ids{};
    uint8_t num_channels{0U};
    /// Requested sample period [us]; rounded to a whole number of ticks.
    float sample_period_us{0.0F};
    uint16_t num_samples{0U};
    uint16_t pretrigger_samples{0U};
  };

  struct BufferInfo {
    uint32_t buffer_bytes{0U};
    uint8_t max_channels{0U};
    /// Frames the buffer holds with one channel; with N channels it holds max_frames / N.
    uint16_t max_frames{0U};
    /// Samples one ReadSamples response carries at most.
    uint8_t max_read_samples{0U};
    /// Period of Tick() [us], 0 if unknown.
    float tick_period_us{0.0F};
  };

  struct SetupCaptureResult {
    ErrorCode error{ErrorCode::kNone};
    /// Sample period used: decimation * tick period [us] (the request if the tick period is unknown).
    float actual_sample_period_us{0.0F};
  };

  struct ArmTriggerConfig {
    bool arm{false};
    uint8_t source_variable_id{0U};
    TriggerMode mode{TriggerMode::kRising};
    float level{0.0F};
    float hysteresis_band{0.0F};
  };

  struct ReadSamplesResult {
    CaptureState state{CaptureState::kIdle};
    uint8_t num_channels{0U};
    uint16_t num_samples{0U};
    uint16_t pretrigger_samples{0U};
    float sample_period_us{0.0F};
    bool end_of_data{false};
    /// Interleaved frames; the first sample_count entries are valid.
    std::array<float, kMaxReadSamples> samples{};
    uint8_t sample_count{0U};

    [[nodiscard]] std::span<const float> Samples() const noexcept { return {samples.data(), sample_count}; }
  };

  struct TriggerEvent {
    uint8_t source_variable_id{0U};
    TriggerMode mode{TriggerMode::kRising};
    float trigger_value{0.0F};
  };

  /**
   * @brief Registers a variable that Tick() samples by reading @p source.
   *
   * Call during start-up, before the first Tick(). The variable ID is the
   * registration index. The name and unit views and the source must outlive
   * the device (string literals and static variables). A float written by
   * another context is read with one 32-bit load, which is atomic on the
   * supported targets.
   *
   * @return false if the registry is full, a string is too long or the name is empty.
   */
  bool RegisterVariable(std::string_view name, std::string_view unit, const float* source) noexcept;

  /// Period of Tick() [us]; used to round the requested sample period to a decimation.
  void SetTickPeriod(float tick_period_us) noexcept;

  [[nodiscard]] uint8_t VariableCount() const noexcept;

  /// Variable @p index; an empty name past the end (cymon.GetVariableList.0.1).
  [[nodiscard]] VariableInfo GetVariable(uint16_t index) const noexcept;

  [[nodiscard]] BufferInfo HandleGetBufferInfo() const noexcept;
  SetupCaptureResult HandleSetupCapture(const CaptureConfig& config) noexcept;
  ErrorCode HandleArmTrigger(const ArmTriggerConfig& config) noexcept;
  /// Status and, once complete, up to @p max_frames frames from @p frame_offset (0 frames = status only).
  [[nodiscard]] ReadSamplesResult HandleReadSamples(uint16_t frame_offset, uint8_t max_frames) const noexcept;

  /// Triggers an armed capture at the next sample (e.g. on a cymon.TriggerEvent of another node).
  void RequestTrigger() noexcept;

  /// Takes the event of the last trigger, once; publish it as cymon.TriggerEvent.0.1.
  bool TakeTriggerEvent(TriggerEvent* event) noexcept;

  [[nodiscard]] CaptureState State() const noexcept;

  /**
   * @brief Advances the capture by one tick; call at the period given to SetTickPeriod().
   * @return true once the capture is complete.
   */
  bool Tick() noexcept;

 private:
  struct Variable {
    std::string_view name;
    std::string_view unit;
    const float* source{nullptr};
  };

  [[nodiscard]] float Read(uint8_t variable_id) const noexcept;

  // Registry, written during start-up only.
  std::array<Variable, kMaxVars> vars_{};
  uint8_t var_count_{0U};
  float tick_period_us_{0.0F};

  // Capture setup: written by the thread while not armed, read by Tick().
  CaptureConfig capture_config_{};
  float actual_sample_period_us_{0.0F};
  uint16_t decimation_{1U};
  uint8_t trigger_source_{0U};
  TriggerMode trigger_mode_{TriggerMode::kRising};

  // Owned by Tick() while armed.
  uint16_t decimation_counter_{0U};
  float prev_trigger_value_{0.0F};
  SampleBuffer buffer_;
  Trigger trigger_;
  float trigger_value_{0.0F};

  std::atomic<CaptureState> state_{CaptureState::kIdle};
  std::atomic<bool> trigger_requested_{false};
  std::atomic<bool> trigger_event_pending_{false};

  static_assert(std::atomic<CaptureState>::is_always_lock_free, "Tick() must be lock-free");
  static_assert(std::atomic<bool>::is_always_lock_free, "Tick() must be lock-free");
};

}  // namespace cymon
