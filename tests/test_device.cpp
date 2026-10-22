// SPDX-License-Identifier: Apache-2.0
// Copyright 2026 Tecnologic

#include <gtest/gtest.h>
#include <array>
#include <vector>
#include "cymon/device.hpp"

namespace cymon {
namespace {

class DeviceTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(device_.RegisterVariable("i_q", "A", &i_q_));
    ASSERT_TRUE(device_.RegisterVariable("speed", "rad/s", &speed_));
  }

  Device::SetupCaptureResult Setup(uint8_t num_channels,
                                   uint16_t num_samples,
                                   uint16_t pretrigger,
                                   float period_us = 100.0F) {
    Device::CaptureConfig config;
    for (uint8_t i = 0; i < num_channels; ++i) {
      config.channel_ids[i] = i;
    }
    config.num_channels = num_channels;
    config.num_samples = num_samples;
    config.pretrigger_samples = pretrigger;
    config.sample_period_us = period_us;
    return device_.HandleSetupCapture(config);
  }

  ErrorCode Arm(TriggerMode mode, float level = 0.0F, uint8_t source = 0U) {
    return device_.HandleArmTrigger(
        {.arm = true, .source_variable_id = source, .mode = mode, .level = level, .hysteresis_band = 0.0F});
  }

  std::vector<float> ReadAll() {
    std::vector<float> samples;
    uint16_t offset = 0U;
    for (;;) {
      const auto result = device_.HandleReadSamples(offset, 255U);
      EXPECT_EQ(result.state, CaptureState::kComplete);
      samples.insert(samples.end(), result.Samples().begin(), result.Samples().end());
      offset = static_cast<uint16_t>(offset + result.sample_count / result.num_channels);
      if (result.end_of_data || result.sample_count == 0U) {
        return samples;
      }
    }
  }

  float i_q_{0.0F};
  float speed_{0.0F};
  Device device_;
};

TEST_F(DeviceTest, RegistryListsVariablesInOrder) {
  EXPECT_EQ(device_.VariableCount(), 2U);
  EXPECT_EQ(device_.GetVariable(1).name, "speed");
  EXPECT_EQ(device_.GetVariable(1).unit, "rad/s");
  EXPECT_TRUE(device_.GetVariable(2).name.empty());
}

TEST_F(DeviceTest, RegistryRejectsBadVariables) {
  const float source = 0.0F;
  EXPECT_FALSE(device_.RegisterVariable("", "A", &source));
  EXPECT_FALSE(device_.RegisterVariable("x", "A", nullptr));
  EXPECT_FALSE(device_.RegisterVariable("abcdefghijklmnopqrstuvwxyz0123456", "A", &source));
  EXPECT_FALSE(device_.RegisterVariable("x", "123456789", &source));
}

TEST(DeviceRegistryTest, RejectsWhenFull) {
  Device device;
  const float source = 0.0F;
  for (std::size_t i = 0; i < kMaxVars; ++i) {
    ASSERT_TRUE(device.RegisterVariable("v", "", &source));
  }
  EXPECT_FALSE(device.RegisterVariable("overflow", "", &source));
}

TEST_F(DeviceTest, BufferInfoReportsLimits) {
  device_.SetTickPeriod(62.5F);
  const auto info = device_.HandleGetBufferInfo();
  EXPECT_EQ(info.buffer_bytes, kMaxBufferBytes);
  EXPECT_EQ(info.max_channels, kMaxChannels);
  EXPECT_EQ(info.max_frames, kMaxBufferBytes / sizeof(float));
  EXPECT_EQ(info.max_read_samples, kMaxReadSamples);
  EXPECT_FLOAT_EQ(info.tick_period_us, 62.5F);
}

TEST_F(DeviceTest, SetupValidatesRequest) {
  EXPECT_EQ(Setup(0, 10, 0).error, ErrorCode::kBadConfig);
  EXPECT_EQ(Setup(static_cast<uint8_t>(kMaxChannels + 1U), 10, 0).error, ErrorCode::kBadConfig);
  EXPECT_EQ(Setup(1, 10, 10).error, ErrorCode::kBadConfig);
  EXPECT_EQ(Setup(3, 10, 0).error, ErrorCode::kBadChannelId);  // only two variables
  EXPECT_EQ(Setup(1, 60000, 0).error, ErrorCode::kBufferTooSmall);
  EXPECT_EQ(device_.State(), CaptureState::kIdle);
  EXPECT_EQ(Setup(2, 10, 2).error, ErrorCode::kNone);
  EXPECT_EQ(device_.State(), CaptureState::kConfigured);
}

TEST_F(DeviceTest, SamplePeriodRoundsToTicks) {
  device_.SetTickPeriod(62.5F);
  EXPECT_FLOAT_EQ(Setup(1, 10, 0, 200.0F).actual_sample_period_us, 187.5F);  // 3.2 ticks -> 3
  EXPECT_FLOAT_EQ(Setup(1, 10, 0, 10.0F).actual_sample_period_us, 62.5F);    // never faster than a tick
}

TEST_F(DeviceTest, SamplePeriodWithoutTickPeriodEchoesRequest) {
  EXPECT_FLOAT_EQ(Setup(1, 10, 0, 75.0F).actual_sample_period_us, 75.0F);
}

TEST_F(DeviceTest, ArmValidatesRequest) {
  EXPECT_EQ(Arm(TriggerMode::kRising), ErrorCode::kNotConfigured);
  ASSERT_EQ(Setup(1, 10, 0).error, ErrorCode::kNone);
  EXPECT_EQ(Arm(TriggerMode::kRising, 0.0F, 99U), ErrorCode::kBadChannelId);
  EXPECT_EQ(Arm(static_cast<TriggerMode>(7)), ErrorCode::kBadConfig);
  EXPECT_EQ(Arm(TriggerMode::kImmediate, 0.0F, 99U), ErrorCode::kNone);  // no source needed
  EXPECT_EQ(device_.State(), CaptureState::kArmed);
}

TEST_F(DeviceTest, SetupWhileArmedIsRefused) {
  ASSERT_EQ(Setup(1, 10, 0).error, ErrorCode::kNone);
  ASSERT_EQ(Arm(TriggerMode::kRising, 1.0F), ErrorCode::kNone);
  EXPECT_EQ(Setup(1, 10, 0).error, ErrorCode::kAlreadyCapturing);
  EXPECT_EQ(device_.HandleArmTrigger({.arm = false}), ErrorCode::kNone);
  EXPECT_EQ(device_.State(), CaptureState::kConfigured);
  EXPECT_EQ(Setup(1, 10, 0).error, ErrorCode::kNone);
}

TEST_F(DeviceTest, ImmediateCaptureRecordsInterleavedFrames) {
  ASSERT_EQ(Setup(2, 4, 0).error, ErrorCode::kNone);
  ASSERT_EQ(Arm(TriggerMode::kImmediate), ErrorCode::kNone);
  for (int i = 0; i < 4; ++i) {
    i_q_ = static_cast<float>(i);
    speed_ = static_cast<float>(10 * i);
    EXPECT_EQ(device_.Tick(), i == 3);
  }
  EXPECT_EQ(device_.State(), CaptureState::kComplete);
  const std::vector<float> expected{0.0F, 0.0F, 1.0F, 10.0F, 2.0F, 20.0F, 3.0F, 30.0F};
  EXPECT_EQ(ReadAll(), expected);
}

TEST_F(DeviceTest, RisingEdgeCaptureWithPretrigger) {
  ASSERT_EQ(Setup(1, 6, 2).error, ErrorCode::kNone);
  ASSERT_EQ(Arm(TriggerMode::kRising, 4.5F), ErrorCode::kNone);
  for (int i = 0; i < 20 && device_.State() != CaptureState::kComplete; ++i) {
    i_q_ = static_cast<float>(i);
    device_.Tick();
  }
  const std::vector<float> expected{3.0F, 4.0F, 5.0F, 6.0F, 7.0F, 8.0F};
  EXPECT_EQ(ReadAll(), expected);

  Device::TriggerEvent event;
  ASSERT_TRUE(device_.TakeTriggerEvent(&event));
  EXPECT_EQ(event.source_variable_id, 0U);
  EXPECT_EQ(event.mode, TriggerMode::kRising);
  EXPECT_FLOAT_EQ(event.trigger_value, 5.0F);
  EXPECT_FALSE(device_.TakeTriggerEvent(&event));
}

TEST_F(DeviceTest, TriggerWaitsForPretriggerToFill) {
  ASSERT_EQ(Setup(1, 4, 3).error, ErrorCode::kNone);
  i_q_ = 1.0F;  // condition true from the first sample on
  ASSERT_EQ(Arm(TriggerMode::kImmediate), ErrorCode::kNone);
  device_.Tick();
  device_.Tick();
  device_.Tick();
  EXPECT_EQ(device_.State(), CaptureState::kArmed);
  device_.Tick();
  EXPECT_EQ(device_.State(), CaptureState::kComplete);
}

TEST_F(DeviceTest, EdgeDuringPretriggerFillIsNotSeenLater) {
  ASSERT_EQ(Setup(1, 4, 2).error, ErrorCode::kNone);
  ASSERT_EQ(Arm(TriggerMode::kRising, 5.0F), ErrorCode::kNone);  // armed at 0
  i_q_ = 10.0F;  // crosses the level while the pre-trigger part fills
  device_.Tick();
  device_.Tick();
  device_.Tick();
  EXPECT_EQ(device_.State(), CaptureState::kArmed);
}

TEST_F(DeviceTest, RequestedTriggerFiresWithoutCondition) {
  ASSERT_EQ(Setup(1, 2, 0).error, ErrorCode::kNone);
  ASSERT_EQ(Arm(TriggerMode::kRising, 100.0F), ErrorCode::kNone);
  device_.Tick();
  EXPECT_EQ(device_.State(), CaptureState::kArmed);
  device_.RequestTrigger();
  device_.Tick();
  EXPECT_EQ(device_.State(), CaptureState::kTriggered);
  device_.Tick();
  EXPECT_EQ(device_.State(), CaptureState::kComplete);
}

TEST_F(DeviceTest, DecimationSkipsTicks) {
  device_.SetTickPeriod(50.0F);
  ASSERT_FLOAT_EQ(Setup(1, 3, 0, 150.0F).actual_sample_period_us, 150.0F);
  ASSERT_EQ(Arm(TriggerMode::kImmediate), ErrorCode::kNone);
  for (int i = 1; i <= 9; ++i) {
    i_q_ = static_cast<float>(i);
    device_.Tick();
  }
  const std::vector<float> expected{3.0F, 6.0F, 9.0F};
  EXPECT_EQ(ReadAll(), expected);
}

TEST_F(DeviceTest, ReadSamplesReportsStatusBeforeCompletion) {
  EXPECT_EQ(device_.HandleReadSamples(0, 4).state, CaptureState::kIdle);
  ASSERT_EQ(Setup(2, 4, 1).error, ErrorCode::kNone);
  ASSERT_EQ(Arm(TriggerMode::kRising, 100.0F), ErrorCode::kNone);
  device_.Tick();
  const auto result = device_.HandleReadSamples(0, 4);
  EXPECT_EQ(result.state, CaptureState::kArmed);
  EXPECT_EQ(result.num_channels, 2U);
  EXPECT_EQ(result.num_samples, 4U);
  EXPECT_EQ(result.pretrigger_samples, 1U);
  EXPECT_EQ(result.sample_count, 0U);
  EXPECT_FALSE(result.end_of_data);
}

TEST_F(DeviceTest, ReadSamplesPagesUpToResponseLimit) {
  ASSERT_EQ(Setup(2, 100, 0).error, ErrorCode::kNone);
  ASSERT_EQ(Arm(TriggerMode::kImmediate), ErrorCode::kNone);
  while (!device_.Tick()) {
  }
  const auto first = device_.HandleReadSamples(0, 255U);
  EXPECT_EQ(first.sample_count, kMaxReadSamples);
  EXPECT_FALSE(first.end_of_data);
  const auto last = device_.HandleReadSamples(96, 255U);
  EXPECT_EQ(last.sample_count, 8U);
  EXPECT_TRUE(last.end_of_data);
  EXPECT_EQ(device_.HandleReadSamples(0, 0U).sample_count, 0U);  // status only
}

}  // namespace
}  // namespace cymon
