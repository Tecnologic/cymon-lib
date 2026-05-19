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
#include <array>
#include <cstdint>
#include <functional>
#include <string_view>

#include "cymon/config.hpp"
#include "cymon/sample_buffer.hpp"
#include "cymon/trigger.hpp"

namespace cymon {

class Device {
 public:
  enum class ErrorCode : uint8_t {
    kNone = 0,
    kBadChannelId = 1,
    kBufferTooSmall = 2,
    kNotConfigured = 3,
    kNotArmed = 4,
    kAlreadyCapturing = 5,
    kTooManyVars = 6,
    kNameTooLong = 7,
    kUnitTooLong = 8,
  };

  struct VariableInfo {
    uint8_t id = 0;
    char name[33] = {};
    char unit[9] = {};
  };

  struct CaptureConfig {
    std::array<uint8_t, kMaxChannels> channel_ids = {};
    uint8_t num_channels = 0;
    float sample_period_us = 0.0F;
    uint16_t num_samples = 0;
    uint16_t pretrigger_samples = 0;
  };

  struct GetBufferInfoResponse {
    uint32_t buffer_bytes = static_cast<uint32_t>(kMaxBufferBytes);
    uint8_t max_channels = static_cast<uint8_t>(kMaxChannels);
    uint16_t max_samples_per_channel = 0;
  };

  struct SetupCaptureResponse {
    bool ok = false;
    ErrorCode error_code = ErrorCode::kNone;
    float actual_sample_period_us = 0.0F;
  };

  struct ArmTriggerConfig {
    bool arm = false;
    uint8_t source_variable_id = 0;
    TriggerMode trigger_mode = TriggerMode::kRising;
    float trigger_level = 0.0F;
    float hysteresis_band = 0.0F;
  };

  struct ArmTriggerResponse {
    bool ok = false;
    ErrorCode error_code = ErrorCode::kNone;
  };

  struct ReadSamplesResponse {
    bool triggered = false;
    bool end_of_data = false;
    uint8_t num_channels = 0;
    std::array<uint8_t, kMaxChannels> channel_ids = {};
    std::array<float, kMaxReadFrames * kMaxChannels> samples = {};
    uint8_t num_samples = 0;  // float count in samples
  };

  Device();

  /// Set actual sample period achieved by hardware timer.
  /// Call after hardware timer setup; returned in SetupCaptureResponse.
  void set_actual_sample_period_us(float us);

  /// Register a named variable with a getter. Returns false if at capacity or
  /// name/unit too long.
  bool RegisterVariable(std::string_view name, std::string_view unit,
                        std::function<float()> getter);

  uint16_t variable_count() const;

  /// Iterator-style: returns false when index is out of range (end of list).
  bool GetVariable(uint16_t index, VariableInfo* info) const;

  GetBufferInfoResponse HandleGetBufferInfo() const;
  SetupCaptureResponse HandleSetupCapture(const CaptureConfig& config);
  ArmTriggerResponse HandleArmTrigger(const ArmTriggerConfig& config);
  ReadSamplesResponse HandleReadSamples(uint16_t frame_offset,
                                        uint8_t max_frames) const;

  /// Call from timer task at configured rate. Returns true when capture is
  /// complete.
  bool Tick();

  /// Force trigger (e.g., from received TriggerEvent message from another
  /// node).
  void ForceTrigger();

 private:
  struct VarEntry {
    char name[33] = {};
    char unit[9] = {};
    std::function<float()> getter;
    bool active = false;
  };

  std::array<VarEntry, kMaxVars> vars_;
  uint8_t var_count_ = 0;
  CaptureConfig capture_config_;
  ArmTriggerConfig trigger_config_;
  float actual_sample_period_us_ = 0.0F;
  bool configured_ = false;
  float prev_trigger_value_ = 0.0F;
  SampleBuffer buffer_;
  Trigger trigger_;
};

}  // namespace cymon
