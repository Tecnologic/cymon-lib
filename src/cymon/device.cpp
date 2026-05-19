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

#include "cymon/device.hpp"

#include <algorithm>
#include <cstring>

namespace cymon {

Device::Device() = default;

void Device::set_actual_sample_period_us(float us) {
  actual_sample_period_us_ = us;
}

bool Device::RegisterVariable(std::string_view name, std::string_view unit,
                              std::function<float()> getter) {
  if (var_count_ >= static_cast<uint8_t>(kMaxVars)) {
    return false;
  }
  if (name.size() > 32U) {
    return false;
  }
  if (unit.size() > 8U) {
    return false;
  }

  VarEntry& entry = vars_[var_count_];
  std::memset(entry.name, 0, sizeof(entry.name));
  std::memset(entry.unit, 0, sizeof(entry.unit));
  std::memcpy(entry.name, name.data(), name.size());
  std::memcpy(entry.unit, unit.data(), unit.size());
  entry.getter = std::move(getter);
  entry.active = true;
  ++var_count_;
  return true;
}

uint16_t Device::variable_count() const {
  return static_cast<uint16_t>(var_count_);
}

bool Device::GetVariable(uint16_t index, VariableInfo* info) const {
  if (index >= var_count_) {
    return false;
  }
  info->id = static_cast<uint8_t>(index);
  std::memcpy(info->name, vars_[index].name, sizeof(info->name));
  std::memcpy(info->unit, vars_[index].unit, sizeof(info->unit));
  return true;
}

Device::GetBufferInfoResponse Device::HandleGetBufferInfo() const {
  GetBufferInfoResponse resp;
  resp.buffer_bytes = static_cast<uint32_t>(kMaxBufferBytes);
  resp.max_channels = static_cast<uint8_t>(kMaxChannels);
  const uint8_t ch = (configured_ && capture_config_.num_channels > 0)
                         ? capture_config_.num_channels
                         : static_cast<uint8_t>(kMaxChannels);
  resp.max_samples_per_channel = static_cast<uint16_t>(
      kMaxBufferBytes / (sizeof(float) * static_cast<std::size_t>(ch)));
  return resp;
}

Device::SetupCaptureResponse Device::HandleSetupCapture(
    const CaptureConfig& config) {
  SetupCaptureResponse resp;

  for (uint8_t i = 0; i < config.num_channels; ++i) {
    if (config.channel_ids[i] >= var_count_) {
      resp.ok = false;
      resp.error_code = ErrorCode::kBadChannelId;
      return resp;
    }
  }

  if (!buffer_.Setup(config.num_channels, config.num_samples,
                     config.pretrigger_samples)) {
    resp.ok = false;
    resp.error_code = ErrorCode::kBufferTooSmall;
    return resp;
  }

  capture_config_ = config;
  configured_ = true;

  resp.ok = true;
  resp.error_code = ErrorCode::kNone;
  resp.actual_sample_period_us = (actual_sample_period_us_ > 0.0F)
                                     ? actual_sample_period_us_
                                     : config.sample_period_us;
  return resp;
}

Device::ArmTriggerResponse Device::HandleArmTrigger(
    const ArmTriggerConfig& config) {
  ArmTriggerResponse resp;

  if (!config.arm) {
    buffer_.Disarm();
    trigger_.Disarm();
    resp.ok = true;
    return resp;
  }

  if (!configured_) {
    resp.ok = false;
    resp.error_code = ErrorCode::kNotConfigured;
    return resp;
  }

  if (config.source_variable_id >= var_count_) {
    resp.ok = false;
    resp.error_code = ErrorCode::kBadChannelId;
    return resp;
  }

  trigger_config_ = config;

  TriggerConfig tc;
  tc.mode = config.trigger_mode;
  tc.level = config.trigger_level;
  tc.hysteresis_band = config.hysteresis_band;
  trigger_.Configure(tc);

  const float current_val = vars_[config.source_variable_id].getter
                                ? vars_[config.source_variable_id].getter()
                                : 0.0F;
  prev_trigger_value_ = current_val;
  trigger_.Arm(current_val);
  buffer_.Arm();

  resp.ok = true;
  return resp;
}

Device::ReadSamplesResponse Device::HandleReadSamples(
    uint16_t frame_offset, uint8_t max_frames) const {
  ReadSamplesResponse resp;

  const uint8_t clamped = static_cast<uint8_t>(
      std::min(static_cast<std::size_t>(max_frames), kMaxReadFrames));

  const uint8_t ch = capture_config_.num_channels;
  resp.num_channels = ch;
  for (uint8_t i = 0; i < ch && i < kMaxChannels; ++i) {
    resp.channel_ids[i] = capture_config_.channel_ids[i];
  }

  resp.triggered = buffer_.WasTriggered();

  if (!resp.triggered) {
    resp.end_of_data = false;
    resp.num_samples = 0;
    return resp;
  }

  const uint16_t frames_read =
      buffer_.ReadFrames(frame_offset, clamped, resp.samples.data());

  resp.num_samples =
      static_cast<uint8_t>(static_cast<std::size_t>(frames_read) * ch);

  const uint16_t total = buffer_.num_samples();
  resp.end_of_data =
      (static_cast<uint32_t>(frame_offset) + frames_read >= total) ||
      buffer_.IsComplete();

  return resp;
}

bool Device::Tick() {
  if (!configured_ || !buffer_.IsArmed()) {
    return buffer_.IsComplete();
  }

  // Evaluate trigger if armed.
  if (trigger_.IsArmed()) {
    const uint8_t src_id = trigger_config_.source_variable_id;
    const float current_val = (src_id < var_count_ && vars_[src_id].getter)
                                  ? vars_[src_id].getter()
                                  : 0.0F;

    if (trigger_.Evaluate(prev_trigger_value_, current_val)) {
      buffer_.Trigger();
    }
    prev_trigger_value_ = current_val;
  }

  // Sample all channels.
  float frame[kMaxChannels] = {};
  const uint8_t ch = capture_config_.num_channels;
  for (uint8_t i = 0; i < ch; ++i) {
    const uint8_t id = capture_config_.channel_ids[i];
    frame[i] =
        (id < var_count_ && vars_[id].getter) ? vars_[id].getter() : 0.0F;
  }
  buffer_.WriteFrame(frame);

  return buffer_.IsComplete();
}

void Device::ForceTrigger() {
  if (buffer_.IsArmed()) {
    buffer_.Trigger();
    trigger_.Disarm();
  }
}

}  // namespace cymon
