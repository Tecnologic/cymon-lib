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

#include "cymon/trigger.hpp"

namespace cymon {

void Trigger::Configure(const TriggerConfig& config) { config_ = config; }

void Trigger::Arm(float initial_value) {
  armed_ = true;
  if (config_.mode == TriggerMode::kHysteresis) {
    awaiting_low_ = (initial_value >= config_.level);
  } else {
    awaiting_low_ = false;
  }
}

void Trigger::Disarm() {
  armed_ = false;
  awaiting_low_ = false;
}

bool Trigger::Evaluate(float prev_value, float current_value) {
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
    case TriggerMode::kHysteresis: {
      const float lower = config_.level - config_.hysteresis_band;
      if (awaiting_low_) {
        if (current_value < lower) {
          awaiting_low_ = false;
        }
      } else {
        if ((prev_value < config_.level) && (current_value >= config_.level)) {
          fired = true;
        }
      }
      break;
    }
  }

  if (fired) {
    armed_ = false;
  }
  return fired;
}

bool Trigger::IsArmed() const { return armed_; }

}  // namespace cymon
