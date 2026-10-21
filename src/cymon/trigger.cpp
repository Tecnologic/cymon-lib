// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic
//
/// @file trigger.cpp
/// @brief Edge and hysteresis trigger of a capture.

#include "cymon/trigger.hpp"

namespace cymon {

void Trigger::Configure(const TriggerConfig& config) noexcept { config_ = config; }

void Trigger::Arm(float initial_value) noexcept {
  armed_ = true;
  awaiting_low_ = (config_.mode == TriggerMode::kHysteresis) && (initial_value >= config_.level);
}

void Trigger::Disarm() noexcept {
  armed_ = false;
  awaiting_low_ = false;
}

bool Trigger::Evaluate(float prev_value, float current_value) noexcept {
  if (!armed_) {
    return false;
  }

  bool fired = false;
  switch (config_.mode) {
    case TriggerMode::kRising:
      fired = (prev_value < config_.level) && (current_value >= config_.level);
      break;
    case TriggerMode::kFalling:
      fired = (prev_value > config_.level) && (current_value <= config_.level);
      break;
    case TriggerMode::kHysteresis:
      if (awaiting_low_) {
        if (current_value < config_.level - config_.hysteresis_band) {
          awaiting_low_ = false;
        }
      } else {
        fired = (prev_value < config_.level) && (current_value >= config_.level);
      }
      break;
    case TriggerMode::kImmediate:
      fired = true;
      break;
  }

  if (fired) {
    armed_ = false;
  }
  return fired;
}

bool Trigger::IsArmed() const noexcept { return armed_; }

}  // namespace cymon
