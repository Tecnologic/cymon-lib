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
#include <cstdint>

namespace cymon {

enum class TriggerMode : uint8_t { kRising = 0, kFalling = 1, kHysteresis = 2 };

struct TriggerConfig {
  TriggerMode mode = TriggerMode::kRising;
  float level = 0.0F;
  float hysteresis_band = 0.0F;
};

class Trigger {
 public:
  void Configure(const TriggerConfig& config);
  // Arm the trigger. initial_value is used to initialize hysteresis state.
  void Arm(float initial_value = 0.0F);
  void Disarm();
  // Returns true if trigger condition fires. Only fires once per arm.
  bool Evaluate(float prev_value, float current_value);
  bool IsArmed() const;

 private:
  TriggerConfig config_;
  bool armed_ = false;
  // hysteresis: waiting for value to drop below lower threshold
  bool awaiting_low_ = false;
};

}  // namespace cymon
