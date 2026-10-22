// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic
//
/// @file device.cpp
/// @brief Device side of the cymon trace protocol: variable registry, capture and trigger.

#include "cymon/device.hpp"
#include <algorithm>
#include <limits>

namespace cymon {

namespace {

constexpr uint32_t kMaxDecimation = std::numeric_limits<uint16_t>::max();

/// Ticks per sample for @p requested_us, rounded to nearest, at least 1.
uint16_t Decimation(float requested_us, float tick_us) noexcept {
  if (!(tick_us > 0.0F) || !(requested_us > tick_us)) {
    return 1U;
  }
  const float kTicks = (requested_us / tick_us) + 0.5F;
  if (kTicks >= static_cast<float>(kMaxDecimation)) {
    return static_cast<uint16_t>(kMaxDecimation);
  }
  return static_cast<uint16_t>(kTicks);
}

bool IsCapturing(CaptureState state) noexcept {
  return state == CaptureState::kArmed || state == CaptureState::kTriggered;
}

}  // namespace

bool Device::RegisterVariable(std::string_view name, std::string_view unit, const float* source) noexcept {
  if (var_count_ >= kMaxVars || name.empty() || name.size() > kMaxNameLength ||
      unit.size() > kMaxUnitLength || source == nullptr) {
    return false;
  }
  vars_[var_count_] = Variable{.name = name, .unit = unit, .source = source};
  ++var_count_;
  return true;
}

void Device::SetTickPeriod(float tick_period_us) noexcept { tick_period_us_ = tick_period_us; }

uint8_t Device::VariableCount() const noexcept { return var_count_; }

Device::VariableInfo Device::GetVariable(uint16_t index) const noexcept {
  if (index >= var_count_) {
    return {};
  }
  return {.name = vars_[index].name, .unit = vars_[index].unit};
}

Device::BufferInfo Device::HandleGetBufferInfo() const noexcept {
  return {.buffer_bytes = static_cast<uint32_t>(kMaxBufferBytes),
          .max_channels = static_cast<uint8_t>(kMaxChannels),
          .max_frames = static_cast<uint16_t>(SampleBuffer::kCapacityFloats),
          .max_read_samples = static_cast<uint8_t>(kMaxReadSamples),
          .tick_period_us = tick_period_us_};
}

Device::SetupCaptureResult Device::HandleSetupCapture(const CaptureConfig& config) noexcept {
  if (IsCapturing(state_.load(std::memory_order_acquire))) {
    return {.error = ErrorCode::kAlreadyCapturing};
  }
  if (config.num_channels == 0U || config.num_channels > kMaxChannels) {
    return {.error = ErrorCode::kBadConfig};
  }
  const auto kChannels = std::span(config.channel_ids).first(config.num_channels);
  if (std::any_of(kChannels.begin(), kChannels.end(), [this](uint8_t variable_id) {
        return variable_id >= var_count_;
      })) {
    return {.error = ErrorCode::kBadChannelId};
  }
  if (config.num_samples == 0U || config.pretrigger_samples >= config.num_samples) {
    return {.error = ErrorCode::kBadConfig};
  }
  if (!buffer_.Setup(config.num_channels, config.num_samples, config.pretrigger_samples)) {
    return {.error = ErrorCode::kBufferTooSmall};
  }

  capture_config_ = config;
  decimation_ = Decimation(config.sample_period_us, tick_period_us_);
  actual_sample_period_us_ =
      (tick_period_us_ > 0.0F) ? static_cast<float>(decimation_) * tick_period_us_ : config.sample_period_us;
  state_.store(CaptureState::kConfigured, std::memory_order_release);
  return {.error = ErrorCode::kNone, .actual_sample_period_us = actual_sample_period_us_};
}

ErrorCode Device::HandleArmTrigger(const ArmTriggerConfig& config) noexcept {
  const CaptureState kState = state_.load(std::memory_order_acquire);
  if (kState == CaptureState::kIdle) {
    return config.arm ? ErrorCode::kNotConfigured : ErrorCode::kNone;
  }
  // Stop Tick() first: from here on the thread owns the capture setup.
  state_.store(CaptureState::kConfigured, std::memory_order_release);
  if (!config.arm) {
    return ErrorCode::kNone;
  }
  if (!IsValidTriggerMode(static_cast<uint8_t>(config.mode))) {
    return ErrorCode::kBadConfig;
  }
  if (config.mode != TriggerMode::kImmediate && config.source_variable_id >= var_count_) {
    return ErrorCode::kBadChannelId;
  }

  trigger_source_ = config.source_variable_id;
  trigger_mode_ = config.mode;
  trigger_.Configure({.mode = config.mode, .level = config.level, .hysteresis_band = config.hysteresis_band});
  prev_trigger_value_ = Read(trigger_source_);
  trigger_.Arm(prev_trigger_value_);
  decimation_counter_ = 0U;
  buffer_.Arm();
  trigger_requested_.store(false, std::memory_order_relaxed);
  state_.store(CaptureState::kArmed, std::memory_order_release);
  return ErrorCode::kNone;
}

Device::ReadSamplesResult Device::HandleReadSamples(uint16_t frame_offset,
                                                    uint8_t max_frames) const noexcept {
  ReadSamplesResult result;
  result.state = state_.load(std::memory_order_acquire);
  if (result.state == CaptureState::kIdle) {
    return result;
  }
  result.num_channels = capture_config_.num_channels;
  result.num_samples = capture_config_.num_samples;
  result.pretrigger_samples = capture_config_.pretrigger_samples;
  result.sample_period_us = actual_sample_period_us_;
  if (result.state != CaptureState::kComplete) {
    return result;
  }

  const uint16_t kFrames = buffer_.ReadFrames(frame_offset, max_frames, result.samples);
  result.sample_count = static_cast<uint8_t>(kFrames * result.num_channels);
  result.end_of_data = static_cast<uint32_t>(frame_offset) + kFrames >= result.num_samples;
  return result;
}

void Device::RequestTrigger() noexcept { trigger_requested_.store(true, std::memory_order_release); }

bool Device::TakeTriggerEvent(TriggerEvent* event) noexcept {
  if (event == nullptr || !trigger_event_pending_.exchange(false, std::memory_order_acquire)) {
    return false;
  }
  *event = {.source_variable_id = trigger_source_, .mode = trigger_mode_, .trigger_value = trigger_value_};
  return true;
}

CaptureState Device::State() const noexcept { return state_.load(std::memory_order_acquire); }

float Device::Read(uint8_t variable_id) const noexcept {
  return (variable_id < var_count_) ? *vars_[variable_id].source : 0.0F;
}

bool Device::Tick() noexcept {
  const CaptureState kState = state_.load(std::memory_order_acquire);
  if (!IsCapturing(kState)) {
    return kState == CaptureState::kComplete;
  }
  if (++decimation_counter_ < decimation_) {
    return false;
  }
  decimation_counter_ = 0U;

  // The trigger is evaluated once the pre-trigger part is recorded, so a
  // complete capture never contains unrecorded frames.
  if (kState == CaptureState::kArmed) {
    const float kValue = Read(trigger_source_);
    if (buffer_.PretriggerFilled() && (trigger_.Evaluate(prev_trigger_value_, kValue) ||
                                       trigger_requested_.exchange(false, std::memory_order_acquire))) {
      trigger_.Disarm();
      buffer_.Trigger();
      trigger_value_ = kValue;
      trigger_event_pending_.store(true, std::memory_order_release);
      state_.store(CaptureState::kTriggered, std::memory_order_release);
    }
    prev_trigger_value_ = kValue;
  }

  std::array<float, kMaxChannels> frame{};
  for (uint8_t i = 0U; i < capture_config_.num_channels; ++i) {
    frame[i] = Read(capture_config_.channel_ids[i]);
  }
  buffer_.WriteFrame(frame);

  if (buffer_.IsComplete()) {
    state_.store(CaptureState::kComplete, std::memory_order_release);
    return true;
  }
  return false;
}

}  // namespace cymon
