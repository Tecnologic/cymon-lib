// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic
//
/// @file trigger.hpp
/// @brief Edge and hysteresis trigger of a capture.

#pragma once

#include <cstdint>

namespace cymon {

/// Trigger condition; the values are those of cymon.TriggerMode.0.1.
enum class TriggerMode : uint8_t {
  kRising = 0,      ///< Fires when the value crosses the level upwards.
  kFalling = 1,     ///< Fires when the value crosses the level downwards.
  kHysteresis = 2,  ///< Rising edge, re-armed only after the value fell below level - band.
  kImmediate = 3,   ///< Fires on the first sample (single shot without condition).
};

/// Returns true if @p raw is a valid TriggerMode value.
[[nodiscard]] constexpr bool IsValidTriggerMode(uint8_t raw) noexcept {
  return raw <= static_cast<uint8_t>(TriggerMode::kImmediate);
}

struct TriggerConfig {
  TriggerMode mode{TriggerMode::kRising};
  float level{0.0F};
  float hysteresis_band{0.0F};
};

/**
 * @brief Evaluates the trigger condition on consecutive samples of one variable.
 *
 * Fires at most once per Arm(). Allocation-free and lock-free; safe to call
 * from an interrupt.
 */
class Trigger {
 public:
  void Configure(const TriggerConfig& config) noexcept;

  /// Arms the trigger. @p initial_value initializes the hysteresis state.
  void Arm(float initial_value = 0.0F) noexcept;

  void Disarm() noexcept;

  /// Returns true when the condition fires; the trigger disarms itself then.
  [[nodiscard]] bool Evaluate(float prev_value, float current_value) noexcept;

  [[nodiscard]] bool IsArmed() const noexcept;

 private:
  TriggerConfig config_;
  bool armed_{false};
  /// Hysteresis: waiting for the value to drop below level - band.
  bool awaiting_low_{false};
};

}  // namespace cymon
